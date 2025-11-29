/* templates.c
 * Copyright (C) 2013 Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Tristan Van Berkom <tristanvb@openismus.com>
 */
#include <gtk/gtk.h>

#ifdef HAVE_UNIX_PRINT_WIDGETS
#  include <gtk/gtkunixprint.h>
#endif

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static gboolean
main_loop_quit_cb (gpointer data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);

  return FALSE;
}

static void
show_and_wait (GtkWidget *widget)
{
  gboolean done = FALSE;

  g_timeout_add (500, main_loop_quit_cb, &done);
  gtk_widget_set_visible (widget, TRUE);
  while (!done)
    g_main_context_iteration (NULL, FALSE);
}

static void
test_dialog_basic (void)
{
  GtkWidget *dialog;

  dialog = gtk_dialog_new ();
  g_assert_true (GTK_IS_DIALOG (dialog));
  g_assert_nonnull (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
test_dialog_override_property (void)
{
  GtkWidget *dialog;

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", 1,
                         NULL);
  g_assert_true (GTK_IS_DIALOG (dialog));

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
test_message_dialog_basic (void)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "Do it hard !");
  g_assert_true (GTK_IS_DIALOG (dialog));
  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
test_about_dialog_basic (void)
{
  GtkWidget *dialog;

  dialog = gtk_about_dialog_new ();
  g_assert_true (GTK_IS_ABOUT_DIALOG (dialog));
  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
test_about_dialog_show (void)
{
  GtkWidget *dialog;

  dialog = gtk_about_dialog_new ();
  g_assert_true (GTK_IS_ABOUT_DIALOG (dialog));
  show_and_wait (dialog);
  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
test_info_bar_basic (void)
{
  GtkWidget *infobar;

  infobar = gtk_info_bar_new ();
  g_assert_true (GTK_IS_INFO_BAR (infobar));
  g_object_unref (g_object_ref_sink (infobar));
}

static void
test_lock_button_basic (void)
{
  GtkWidget *button;
  GPermission *permission;

  permission = g_simple_permission_new (TRUE);
  button = gtk_lock_button_new (permission);
  g_assert_true (GTK_IS_LOCK_BUTTON (button));
  g_object_unref (g_object_ref_sink (button));
  g_object_unref (permission);
}

static void
test_assistant_basic (void)
{
  GtkWidget *widget;

  widget = gtk_assistant_new ();
  g_assert_true (GTK_IS_ASSISTANT (widget));
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_assistant_show (void)
{
  GtkWidget *widget;

  widget = gtk_assistant_new ();
  g_assert_true (GTK_IS_ASSISTANT (widget));
  show_and_wait (widget);
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_scale_button_basic (void)
{
  GtkWidget *widget;

  widget = gtk_scale_button_new (0, 100, 10, NULL);
  g_assert_true (GTK_IS_SCALE_BUTTON (widget));
  g_object_unref (g_object_ref_sink (widget));
}

static void
test_volume_button_basic (void)
{
  GtkWidget *widget;

  widget = gtk_volume_button_new ();
  g_assert_true (GTK_IS_VOLUME_BUTTON (widget));
  g_object_unref (g_object_ref_sink (widget));
}

static void
test_statusbar_basic (void)
{
  GtkWidget *widget;

  widget = gtk_statusbar_new ();
  g_assert_true (GTK_IS_STATUSBAR (widget));
  g_object_unref (g_object_ref_sink (widget));
}

static void
test_search_bar_basic (void)
{
  GtkWidget *widget;

  widget = gtk_search_bar_new ();
  g_assert_true (GTK_IS_SEARCH_BAR (widget));
  g_object_unref (g_object_ref_sink (widget));
}

static void
test_action_bar_basic (void)
{
  GtkWidget *widget;

  widget = gtk_action_bar_new ();
  g_assert_true (GTK_IS_ACTION_BAR (widget));
  g_object_unref (g_object_ref_sink (widget));
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
test_app_chooser_widget_basic (void)
{
  GtkWidget *widget;

  widget = gtk_app_chooser_widget_new (NULL);
  g_assert_true (GTK_IS_APP_CHOOSER_WIDGET (widget));
  g_object_unref (g_object_ref_sink (widget));
}

static void
test_app_chooser_dialog_basic (void)
{
  GtkWidget *widget;
  gboolean done = FALSE;

  widget = gtk_app_chooser_dialog_new_for_content_type (NULL, 0, "text/plain");
  g_assert_true (GTK_IS_APP_CHOOSER_DIALOG (widget));

  /* GtkAppChooserDialog bug, if destroyed before spinning 
   * the main context then app_chooser_online_get_default_ready_cb()
   * will be eventually called and segfault.
   */
  g_timeout_add (500, main_loop_quit_cb, &done);
  while (!done)
    g_main_context_iteration (NULL,  TRUE);
  gtk_window_destroy (GTK_WINDOW (widget));
}

G_GNUC_END_IGNORE_DEPRECATIONS

static void
test_color_chooser_dialog_basic (void)
{
  GtkWidget *widget;

  /* This test also tests the internal GtkColorEditor widget */
  widget = gtk_color_chooser_dialog_new (NULL, NULL);
  g_assert_true (GTK_IS_COLOR_CHOOSER_DIALOG (widget));
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_color_chooser_dialog_show (void)
{
  GtkWidget *widget;

  /* This test also tests the internal GtkColorEditor widget */
  widget = gtk_color_chooser_dialog_new (NULL, NULL);
  g_assert_true (GTK_IS_COLOR_CHOOSER_DIALOG (widget));
  show_and_wait (widget);
  gtk_window_destroy (GTK_WINDOW (widget));
}

/* Avoid warnings from GVFS-RemoteVolumeMonitor */
static gboolean
ignore_gvfs_warning (const char *log_domain,
                     GLogLevelFlags log_level,
                     const char *message,
                     gpointer user_data)
{
  if (g_strcmp0 (log_domain, "GVFS-RemoteVolumeMonitor") == 0)
    return FALSE;

  return TRUE;
}

static void
test_file_chooser_widget_basic (void)
{
  GtkWidget *widget;
  gboolean done = FALSE;

  /* This test also tests the internal GtkPathBar widget */
  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);

  widget = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  g_assert_true (GTK_IS_FILE_CHOOSER_WIDGET (widget));

  /* XXX BUG:
   *
   * Spin the mainloop for a bit, this allows the file operations
   * to complete, GtkFileChooserWidget has a bug where it leaks
   * GtkTreeRowReferences to the internal shortcuts_model
   *
   * Since we assert all automated children are finalized we
   * can catch this
   */
  g_timeout_add (100, main_loop_quit_cb, &done);
  while (!done)
    g_main_context_iteration (NULL,  TRUE);

  g_object_unref (g_object_ref_sink (widget));
}

static void
test_file_chooser_dialog_basic (void)
{
  GtkWidget *widget;
  gboolean done;

  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);

  widget = gtk_file_chooser_dialog_new ("The Dialog", NULL,
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        "_OK", GTK_RESPONSE_OK,
                                        NULL);

  g_assert_true (GTK_IS_FILE_CHOOSER_DIALOG (widget));
  done = FALSE;
  g_timeout_add (100, main_loop_quit_cb, &done);
  while (!done)
    g_main_context_iteration (NULL,  TRUE);

  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_file_chooser_dialog_show (void)
{
  GtkWidget *widget;

  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);

  widget = gtk_file_chooser_dialog_new ("The Dialog", NULL,
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        "_OK", GTK_RESPONSE_OK,
                                        NULL);

  g_assert_true (GTK_IS_FILE_CHOOSER_DIALOG (widget));
  show_and_wait (widget);
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_font_button_basic (void)
{
  GtkWidget *widget;

  widget = gtk_font_button_new ();
  g_assert_true (GTK_IS_FONT_BUTTON (widget));
  g_object_unref (g_object_ref_sink (widget));
}

static void
test_font_chooser_widget_basic (void)
{
  GtkWidget *widget;

  widget = gtk_font_chooser_widget_new ();
  g_assert_true (GTK_IS_FONT_CHOOSER_WIDGET (widget));
  g_object_unref (g_object_ref_sink (widget));
}

static void
test_font_chooser_dialog_basic (void)
{
  GtkWidget *widget;

  widget = gtk_font_chooser_dialog_new ("Choose a font !", NULL);
  g_assert_true (GTK_IS_FONT_CHOOSER_DIALOG (widget));
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_font_chooser_dialog_show (void)
{
  GtkWidget *widget;

  widget = gtk_font_chooser_dialog_new ("Choose a font !", NULL);
  g_assert_true (GTK_IS_FONT_CHOOSER_DIALOG (widget));
  show_and_wait (widget);
  gtk_window_destroy (GTK_WINDOW (widget));
}

#ifdef HAVE_UNIX_PRINT_WIDGETS
static void
test_page_setup_unix_dialog_basic (void)
{
  GtkWidget *widget;

  widget = gtk_page_setup_unix_dialog_new ("Setup your Page !", NULL);
  g_assert_true (GTK_IS_PAGE_SETUP_UNIX_DIALOG (widget));
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_page_setup_unix_dialog_show (void)
{
  GtkWidget *widget;

  widget = gtk_page_setup_unix_dialog_new ("Setup your Page !", NULL);
  g_assert_true (GTK_IS_PAGE_SETUP_UNIX_DIALOG (widget));
  show_and_wait (widget);
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_print_unix_dialog_basic (void)
{
  GtkWidget *widget;

  widget = gtk_print_unix_dialog_new ("Go Print !", NULL);
  g_assert_true (GTK_IS_PRINT_UNIX_DIALOG (widget));
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_print_unix_dialog_show (void)
{
  GtkWidget *widget;

  widget = gtk_print_unix_dialog_new ("Go Print !", NULL);
  g_assert_true (GTK_IS_PRINT_UNIX_DIALOG (widget));
  show_and_wait (widget);
  gtk_window_destroy (GTK_WINDOW (widget));
}
#endif

int
main (int argc, char **argv)
{
  /* These must be set before gtk_test_init */
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  /* initialize test program */
  gtk_test_init (&argc, &argv);

  /* This environment variable cooperates with widget dispose()
   * to assert that all automated compoenents are properly finalized
   * when a given composite widget is destroyed.
   */
  g_assert_true (g_setenv ("GTK_WIDGET_ASSERT_COMPONENTS", "1", TRUE));

  g_test_add_func ("/template/GtkDialog/basic", test_dialog_basic);
  g_test_add_func ("/template/GtkDialog/OverrideProperty", test_dialog_override_property);
  g_test_add_func ("/template/GtkMessageDialog/basic", test_message_dialog_basic);
  g_test_add_func ("/template/GtkAboutDialog/basic", test_about_dialog_basic);
  g_test_add_func ("/template/GtkAboutDialog/show", test_about_dialog_show);
  g_test_add_func ("/template/GtkInfoBar/basic", test_info_bar_basic);
  g_test_add_func ("/template/GtkLockButton/basic", test_lock_button_basic);
  g_test_add_func ("/template/GtkAssistant/basic", test_assistant_basic);
  g_test_add_func ("/template/GtkAssistant/show", test_assistant_show);
  g_test_add_func ("/template/GtkScaleButton/basic", test_scale_button_basic);
  g_test_add_func ("/template/GtkVolumeButton/basic", test_volume_button_basic);
  g_test_add_func ("/template/GtkStatusBar/basic", test_statusbar_basic);
  g_test_add_func ("/template/GtkSearchBar/basic", test_search_bar_basic);
  g_test_add_func ("/template/GtkActionBar/basic", test_action_bar_basic);
  g_test_add_func ("/template/GtkAppChooserWidget/basic", test_app_chooser_widget_basic);
  g_test_add_func ("/template/GtkAppChooserDialog/basic", test_app_chooser_dialog_basic);
  g_test_add_func ("/template/GtkColorChooserDialog/basic", test_color_chooser_dialog_basic);
  g_test_add_func ("/template/GtkColorChooserDialog/show", test_color_chooser_dialog_show);
  g_test_add_func ("/template/GtkFileChooserWidget/basic", test_file_chooser_widget_basic);
  g_test_add_func ("/template/GtkFileChooserDialog/basic", test_file_chooser_dialog_basic);
  g_test_add_func ("/template/GtkFileChooserDialog/show", test_file_chooser_dialog_show);
  g_test_add_func ("/template/GtkFontButton/basic", test_font_button_basic);
  g_test_add_func ("/template/GtkFontChooserWidget/basic", test_font_chooser_widget_basic);
  g_test_add_func ("/template/GtkFontChooserDialog/basic", test_font_chooser_dialog_basic);
  g_test_add_func ("/template/GtkFontChooserDialog/show", test_font_chooser_dialog_show);

#ifdef HAVE_UNIX_PRINT_WIDGETS
  g_test_add_func ("/template/GtkPageSetupUnixDialog/basic", test_page_setup_unix_dialog_basic);
  g_test_add_func ("/template/GtkPageSetupUnixDialog/show", test_page_setup_unix_dialog_show);
  g_test_add_func ("/template/GtkPrintUnixDialog/basic", test_print_unix_dialog_basic);
  g_test_add_func ("/template/GtkPrintUnixDialog/show", test_print_unix_dialog_show);
#endif

  return g_test_run();
}
