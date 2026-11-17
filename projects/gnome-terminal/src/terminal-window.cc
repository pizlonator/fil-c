/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2007, 2008, 2009, 2011, 2017 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <uuid.h>

#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-enums.hh"
#include "terminal-headerbar.hh"
#include "terminal-icon-button.hh"
#include "terminal-intl.hh"
#include "terminal-mdi-container.hh"
#include "terminal-menu-button.hh"
#include "terminal-notebook.hh"
#include "terminal-schemas.hh"
#include "terminal-screen-container.hh"
#include "terminal-search-popover.hh"
#include "terminal-tab-label.hh"
#include "terminal-util.hh"
#include "terminal-window.hh"
#include "terminal-libgsystem.hh"

struct _TerminalWindowPrivate
{
  char *uuid;

  GtkClipboard *clipboard;

  TerminalScreenPopupInfo *popup_info;

  GtkWidget *menubar;
  TerminalMdiContainer *mdi_container;
  GtkWidget *main_vbox;
  GtkWidget* ask_default_infobar;
  TerminalScreen *active_screen;

  /* Size of a character cell in pixels */
  int old_char_width;
  int old_char_height;

  /* Width and height added to the actual terminal grid by "chrome" inside
   * what was traditionally the X11 window: menu bar, title bar,
   * style-provided padding. This must be included when resizing the window
   * and also included in geometry hints. */
  int old_chrome_width;
  int old_chrome_height;

  /* Width and height added to the window by client-side decorations.
   * This must be included in geometry hints but must not be included when
   * resizing the window. */
  int old_csd_width;
  int old_csd_height;

  /* Width and height of the padding around the geometry widget. */
  int old_padding_width;
  int old_padding_height;

  void *old_geometry_widget; /* only used for pointer value as it may be freed */

  /* For restoring hints after unmaximizing etc */
  GdkGeometry hints;
  GdkWindowState window_state;

  GtkWidget *confirm_close_dialog;
  TerminalSearchPopover *search_popover;

  guint use_default_menubar_visibility : 1;

  guint disposed : 1;
  guint present_on_insert : 1;

  guint realized : 1;
};

#define TERMINAL_WINDOW_CSS_NAME "terminal-window"

#define MIN_WIDTH_CHARS 4
#define MIN_HEIGHT_CHARS 1

#if 1
/*
 * We don't want to enable content saving until vte supports it async.
 * So we disable this code for stable versions.
 */
#include "terminal-version.hh"

#if (TERMINAL_MINOR_VERSION & 1) != 0
#define ENABLE_SAVE
#else
#undef ENABLE_SAVE
#endif
#endif

/* See bug #789356 and issue gnome-terminal#129*/
static inline constexpr auto
window_state_is_snapped(GdkWindowState state) noexcept
{
  return (state & (GDK_WINDOW_STATE_FULLSCREEN |
                   GDK_WINDOW_STATE_MAXIMIZED |
                   GDK_WINDOW_STATE_BOTTOM_TILED |
                   GDK_WINDOW_STATE_LEFT_TILED |
                   GDK_WINDOW_STATE_RIGHT_TILED |
                   GDK_WINDOW_STATE_TOP_TILED |
                   GDK_WINDOW_STATE_TILED)) != 0;
}

static void terminal_window_dispose     (GObject             *object);
static void terminal_window_finalize    (GObject             *object);
static gboolean terminal_window_state_event (GtkWidget            *widget,
                                             GdkEventWindowState  *event);

static gboolean terminal_window_delete_event (GtkWidget *widget,
                                              GdkEvent *event,
                                              gpointer data);

static gboolean notebook_button_press_cb     (GtkWidget *notebook,
                                              GdkEventButton *event,
                                              TerminalWindow *window);
static gboolean notebook_popup_menu_cb       (GtkWidget *notebook,
                                              TerminalWindow *window);
static void mdi_screen_switched_cb (TerminalMdiContainer *container,
                                    TerminalScreen *old_active_screen,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void mdi_screen_added_cb    (TerminalMdiContainer *container,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void mdi_screen_removed_cb  (TerminalMdiContainer *container,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void mdi_screens_reordered_cb (TerminalMdiContainer *container,
                                      TerminalWindow  *window);
static void screen_close_request_cb (TerminalMdiContainer *container,
                                     TerminalScreen *screen,
                                     TerminalWindow *window);

/* Menu action callbacks */
static gboolean find_larger_zoom_factor  (double *zoom);
static gboolean find_smaller_zoom_factor (double *zoom);
static void terminal_window_update_zoom_sensitivity (TerminalWindow *window);
static void terminal_window_update_search_sensitivity (TerminalScreen *screen,
                                                       TerminalWindow *window);
static void terminal_window_update_paste_sensitivity (TerminalWindow *window);

static void terminal_window_show (GtkWidget *widget);

static gboolean confirm_close_window_or_tab (TerminalWindow *window,
                                             TerminalScreen *screen);

G_DEFINE_TYPE (TerminalWindow, terminal_window, GTK_TYPE_APPLICATION_WINDOW)

/* Zoom helpers */

static const double zoom_factors[] = {
  TERMINAL_SCALE_MINIMUM,
  TERMINAL_SCALE_XXXXX_SMALL,
  TERMINAL_SCALE_XXXX_SMALL,
  TERMINAL_SCALE_XXX_SMALL,
  PANGO_SCALE_XX_SMALL,
  PANGO_SCALE_X_SMALL,
  PANGO_SCALE_SMALL,
  PANGO_SCALE_MEDIUM,
  PANGO_SCALE_LARGE,
  PANGO_SCALE_X_LARGE,
  PANGO_SCALE_XX_LARGE,
  TERMINAL_SCALE_XXX_LARGE,
  TERMINAL_SCALE_XXXX_LARGE,
  TERMINAL_SCALE_XXXXX_LARGE,
  TERMINAL_SCALE_MAXIMUM
};

static gboolean
find_larger_zoom_factor (double *zoom)
{
  double current = *zoom;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (zoom_factors); ++i)
    {
      /* Find a font that's larger than this one */
      if ((zoom_factors[i] - current) > 1e-6)
        {
          *zoom = zoom_factors[i];
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
find_smaller_zoom_factor (double *zoom)
{
  double current = *zoom;
  int i;

  i = (int) G_N_ELEMENTS (zoom_factors) - 1;
  while (i >= 0)
    {
      /* Find a font that's smaller than this one */
      if ((current - zoom_factors[i]) > 1e-6)
        {
          *zoom = zoom_factors[i];
          return TRUE;
        }

      --i;
    }

  return FALSE;
}

static inline GSimpleAction *
lookup_action (TerminalWindow *window,
               const char *name)
{
  GAction *action;

  action = g_action_map_lookup_action (G_ACTION_MAP (window), name);
  g_return_val_if_fail (action != nullptr, nullptr);

  return G_SIMPLE_ACTION (action);
}

/* Context menu helpers */

/* We don't want context menus to show accelerators.
 * Setting the menu's accel group and/or accel path to nullptr
 * unfortunately doesn't hide accelerators; we need to walk
 * the menu items and remove the accelerators on each,
 * manually.
 */
static void
popup_menu_remove_accelerators (GtkWidget *menu)
{
  gs_free_list GList *menu_items;
  GList *l;

  menu_items = gtk_container_get_children (GTK_CONTAINER (menu));
  for (l = menu_items; l != nullptr; l = l ->next) {
    GtkMenuItem *item = (GtkMenuItem*) (l->data);
    GtkWidget *label, *submenu;

    if (!GTK_IS_MENU_ITEM (item))
      continue;

    if (GTK_IS_ACCEL_LABEL ((label = gtk_bin_get_child (GTK_BIN (item)))))
      gtk_accel_label_set_accel (GTK_ACCEL_LABEL (label), 0, GdkModifierType(0));

    /* Recurse into submenus */
    if ((submenu = gtk_menu_item_get_submenu (item)))
      popup_menu_remove_accelerators (submenu);
  }
}

/* Because we're using gtk_menu_attach_to_widget(), the attach
 * widget holds a strong reference to the menu, causing it not to
 * be automatically destroyed once popped down. So we need to
 * detach the menu from the attach widget manually, which will
 * cause the menu to be destroyed. We cannot do so in the
 * "deactivate" handler however, since that causes the menu
 * item activation to be lost. The "selection-done" signal
 * appears to be the right place.
 */

static void
popup_menu_destroy_cb (GtkWidget *menu,
                       gpointer user_data)
{
  /* g_printerr ("Menu %p destroyed!\n", menu); */
}

static void
popup_menu_selection_done_cb (GtkMenu *menu,
                              gpointer user_data)
{
  g_signal_handlers_disconnect_by_func(menu,
				       (void*)popup_menu_selection_done_cb,
				       user_data);

  /* g_printerr ("selection-done %p\n", menu); */

  /* This will remove the ref from the attach widget widget, and destroy the menu */
  if (gtk_menu_get_attach_widget (menu) != nullptr)
    gtk_menu_detach (menu);
}

static void
popup_menu_detach_cb (GtkWidget *attach_widget,
                      GtkMenu *menu)
{
  gtk_menu_shell_deactivate (GTK_MENU_SHELL (menu));
}

static GtkWidget *
context_menu_new (GMenuModel *menu,
                  GtkWidget *widget)
{
  GtkWidget *popup_menu;

  popup_menu = gtk_menu_new_from_model (menu);
  gtk_style_context_add_class (gtk_widget_get_style_context (popup_menu),
                               GTK_STYLE_CLASS_CONTEXT_MENU);
  gtk_menu_attach_to_widget (GTK_MENU (popup_menu), widget,
                             (GtkMenuDetachFunc)popup_menu_detach_cb);

  popup_menu_remove_accelerators (popup_menu);

  /* Staggered destruction */
  g_signal_connect (popup_menu, "selection-done",
                    G_CALLBACK (popup_menu_selection_done_cb), widget);
  g_signal_connect (popup_menu, "destroy",
                    G_CALLBACK (popup_menu_destroy_cb), widget);

  return popup_menu;
}

/* GAction callbacks */

static void
action_new_terminal_cb (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  TerminalSettingsList *profiles_list;
  gs_unref_object GSettings *profile = nullptr;
  gboolean can_toggle = FALSE;

  g_assert (TERMINAL_IS_WINDOW (window));

  app = terminal_app_get ();

  const char *mode_str, *uuid_str;
  g_variant_get (parameter, "(&s&s)", &mode_str, &uuid_str);

  TerminalNewTerminalMode mode;
  if (g_str_equal (mode_str, "tab"))
    mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
  else if (g_str_equal (mode_str, "window"))
    mode = TERMINAL_NEW_TERMINAL_MODE_WINDOW;
  else if (g_str_equal (mode_str, "tab-default")) {
    mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
    can_toggle = TRUE;
  } else {
    mode = TerminalNewTerminalMode(g_settings_get_enum (terminal_app_get_global_settings (app),
						      TERMINAL_SETTING_NEW_TERMINAL_MODE_KEY));
    can_toggle = TRUE;
  }

  if (can_toggle) {
    GdkEvent *event = gtk_get_current_event ();
    if (event != nullptr) {
      GdkModifierType modifiers;

      if ((gdk_event_get_state (event, &modifiers) &&
           (modifiers & gtk_accelerator_get_default_mod_mask () & GDK_CONTROL_MASK))) {
        /* Invert */
        if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
          mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
        else
          mode = TERMINAL_NEW_TERMINAL_MODE_WINDOW;
      }
      gdk_event_free (event);
    }
  }

  TerminalScreen *parent_screen = priv->active_screen;

  profiles_list = terminal_app_get_profiles_list (app);
  if (g_str_equal (uuid_str, "current"))
    profile = terminal_screen_ref_profile (parent_screen);
  else if (g_str_equal (uuid_str, "default"))
    profile = terminal_settings_list_ref_default_child (profiles_list);
  else
    profile = terminal_settings_list_ref_child (profiles_list, uuid_str);

  if (profile == nullptr)
    return;

  if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
    window = terminal_window_new (G_APPLICATION (app));

  TerminalScreen *screen = terminal_screen_new (profile,
                                                nullptr /* title */,
                                                1.0);

  /* Now add the new screen to the window */
  terminal_window_add_screen (window, screen, -1);
  terminal_window_switch_screen (window, screen);
  gtk_widget_grab_focus (GTK_WIDGET (screen));

  /* Start child process, if possible by using the same args as the parent screen */
  terminal_screen_reexec_from_screen (screen, parent_screen, nullptr, nullptr);

  if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
    gtk_window_present (GTK_WINDOW (window));
}

#ifdef ENABLE_SAVE

static void
save_contents_dialog_on_response (GtkDialog *dialog,
				  int response_id,
				  gpointer user_data)
{
  VteTerminal *terminal = (VteTerminal*)user_data;
  GtkWindow *parent;
  gs_free gchar *filename_uri = nullptr;
  gs_unref_object GFile *file = nullptr;
  GOutputStream *stream;
  gs_free_error GError *error = nullptr;

  if (response_id != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      return;
    }

  parent = (GtkWindow*) gtk_widget_get_ancestor (GTK_WIDGET (terminal), GTK_TYPE_WINDOW);
  filename_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (filename_uri == nullptr)
    return;

  file = g_file_new_for_uri (filename_uri);
  stream = G_OUTPUT_STREAM (g_file_replace (file, nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, &error));

  if (stream)
    {
      /* XXX
       * FIXME
       * This is a sync operation.
       * Should be replaced with the async version when vte implements that.
       */
      vte_terminal_write_contents_sync (terminal, stream,
					VTE_WRITE_DEFAULT,
					nullptr, &error);
      g_object_unref (stream);
    }

  if (error)
    {
      terminal_util_show_error_dialog (parent, nullptr, error,
				       "%s", _("Could not save contents"));
    }
}

static void
action_save_contents_cb (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  GtkWidget *dialog = nullptr;
  TerminalWindowPrivate *priv = window->priv;
  VteTerminal *terminal;

  if (priv->active_screen == nullptr)
    return;

  terminal = VTE_TERMINAL (priv->active_screen);
  g_return_if_fail (VTE_IS_TERMINAL (terminal));

  dialog = gtk_file_chooser_dialog_new (_("Save as…"),
                                        GTK_WINDOW(window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_ACCEPT,
                                        nullptr);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  g_signal_connect (dialog, "response", G_CALLBACK (save_contents_dialog_on_response), terminal);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (terminal_util_dialog_response_on_delete), nullptr);

  gtk_window_present (GTK_WINDOW (dialog));
}

#endif /* ENABLE_SAVE */

#ifdef ENABLE_PRINT

static void
print_begin_cb (GtkPrintOperation *op,
                GtkPrintContext *context,
                TerminalApp *app)
{
  GtkPrintSettings *settings;
  GtkPageSetup *page_setup;

  /* Don't save if the print dialogue was cancelled */
  if (gtk_print_operation_get_status(op) == GTK_PRINT_STATUS_FINISHED_ABORTED)
    return;

  settings = gtk_print_operation_get_print_settings (op);
  page_setup = gtk_print_operation_get_default_page_setup (op);
  terminal_util_save_print_settings (settings, page_setup);
}

static void
print_done_cb (GtkPrintOperation *op,
               GtkPrintOperationResult result,
               TerminalWindow *window)
{
  if (result != GTK_PRINT_OPERATION_RESULT_ERROR)
    return;

  /* FIXME: show error */
}

static void
action_print_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  gs_unref_object GtkPrintSettings *settings = nullptr;
  gs_unref_object GtkPageSetup *page_setup = nullptr;
  gs_unref_object GtkPrintOperation *op = nullptr;
  gs_free_error GError *error = nullptr;
  GtkPrintOperationResult result;

  if (priv->active_screen == nullptr)
    return;

  op = vte_print_operation_new (VTE_TERMINAL (priv->active_screen),
                                VTE_PRINT_OPERATION_DEFAULT /* flags */);
  if (op == nullptr)
    return;

  terminal_util_load_print_settings (&settings, &page_setup);
  if (settings != nullptr)
    gtk_print_operation_set_print_settings (op, settings);
  if (page_setup != nullptr)
    gtk_print_operation_set_default_page_setup (op, page_setup);

  g_signal_connect (op, "begin-print", G_CALLBACK (print_begin_cb), window);
  g_signal_connect (op, "done", G_CALLBACK (print_done_cb), window);

  /* FIXME: show progress better */

  result = gtk_print_operation_run (op,
                                    /* this is the only supported one: */
                                    GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                    GTK_WINDOW (window),
                                    &error);
  /* VtePrintOperation always runs async */
  g_assert_cmpint (result, ==, GTK_PRINT_OPERATION_RESULT_IN_PROGRESS);
}

#endif /* ENABLE_PRINT */

#ifdef ENABLE_EXPORT

static void
action_export_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  gs_unref_object VteExportOperation *op = nullptr;
  gs_free_error GError *error = nullptr;

  if (priv->active_screen == nullptr)
    return;

  op = vte_export_operation_new (VTE_TERMINAL (priv->active_screen),
                                 TRUE /* interactive */,
                                 VTE_EXPORT_FORMAT_ASK /* allow user to choose export format */,
                                 nullptr, nullptr /* GSettings & key to load/store default directory from, FIXME */,
                                 nullptr, nullptr /* progress callback & user data, FIXME */);
  if (op == nullptr)
    return;

  /* FIXME: show progress better */

  vte_export_operation_run_async (op, GTK_WINDOW (window), nullptr /* cancellable */);
}

#endif /* ENABLE_EXPORT */

static void
action_close_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *screen;
  const char *mode_str;

  g_assert_nonnull (parameter);
  g_variant_get (parameter, "&s", &mode_str);

  if (g_str_equal (mode_str, "tab"))
    screen = priv->active_screen;
  else if (g_str_equal (mode_str, "window"))
    screen = nullptr;
  else
    return;

  if (confirm_close_window_or_tab (window, screen))
    return;

  if (screen)
    terminal_window_remove_screen (window, screen);
  else
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
action_copy_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  const char *format_str;
  VteFormat format;

  if (priv->active_screen == nullptr)
    return;

  g_assert_nonnull (parameter);
  g_variant_get (parameter, "&s", &format_str);

  if (g_str_equal (format_str, "text"))
    format = VTE_FORMAT_TEXT;
  else if (g_str_equal (format_str, "html"))
    format = VTE_FORMAT_HTML;
  else
    return;

  vte_terminal_copy_clipboard_format (VTE_TERMINAL (priv->active_screen), format);
}

/* Clipboard helpers */

typedef struct {
  GWeakRef screen_weak_ref;
} PasteData;

static void
clipboard_uris_received_cb (GtkClipboard *clipboard,
                            /* const */ char **uris,
                            PasteData *data)
{
  gs_unref_object TerminalScreen *screen = nullptr;

  if (uris != nullptr && uris[0] != nullptr &&
      (screen = (TerminalScreen*)g_weak_ref_get (&data->screen_weak_ref))) {
    gs_free char *text;
    gsize len;

    /* This potentially modifies the strings in |uris| but that's ok */
    terminal_util_transform_uris_to_quoted_fuse_paths (uris);
    text = terminal_util_concat_uris (uris, &len);

    terminal_screen_paste_text (screen, text, len);
  }

  g_weak_ref_clear (&data->screen_weak_ref);
  g_slice_free (PasteData, data);
}

static void
request_clipboard_contents_for_paste (TerminalWindow *window,
                                      gboolean paste_as_uris)
{
  TerminalWindowPrivate *priv = window->priv;
  GdkAtom *targets;
  int n_targets;

  if (priv->active_screen == nullptr)
    return;

  targets = terminal_app_get_clipboard_targets (terminal_app_get (),
                                                priv->clipboard,
                                                &n_targets);
  if (targets == nullptr)
    return;

  if (paste_as_uris && gtk_targets_include_uri (targets, n_targets)) {
    PasteData *data = g_slice_new (PasteData);
    g_weak_ref_init (&data->screen_weak_ref, priv->active_screen);

    gtk_clipboard_request_uris (priv->clipboard,
                                (GtkClipboardURIReceivedFunc) clipboard_uris_received_cb,
                                data);
    return;
  } else if (gtk_targets_include_text (targets, n_targets)) {
    vte_terminal_paste_clipboard (VTE_TERMINAL (priv->active_screen));
  }
}

static void
action_paste_text_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  request_clipboard_contents_for_paste (window, FALSE);
}

static void
action_paste_uris_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  request_clipboard_contents_for_paste (window, TRUE);
}

static void
action_select_all_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == nullptr)
    return;

  vte_terminal_select_all (VTE_TERMINAL (priv->active_screen));
}

static void
action_reset_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  g_assert_nonnull (parameter);

  if (priv->active_screen == nullptr)
    return;

  vte_terminal_reset (VTE_TERMINAL (priv->active_screen),
                      TRUE,
                      g_variant_get_boolean (parameter));
}

static void
tab_switch_relative (TerminalWindow *window,
                     int change)
{
  TerminalWindowPrivate *priv = window->priv;
  int n_screens, value;

  n_screens = terminal_mdi_container_get_n_screens (priv->mdi_container);
  value = terminal_mdi_container_get_active_screen_num (priv->mdi_container) + change;

  gboolean keynav_wrap_around;
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                "gtk-keynav-wrap-around", &keynav_wrap_around,
                nullptr);
  if (keynav_wrap_around) {
    if (value < 0)
      value += n_screens;
    else if (value >= n_screens)
      value -= n_screens;
  }

  if (value < 0 || value >= n_screens)
    return;

  g_action_change_state (G_ACTION (lookup_action (window, "active-tab")),
                         g_variant_new_int32 (value));
}

static void
action_tab_switch_left_cb (GSimpleAction *action,
                           GVariant *parameter,
                           gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  tab_switch_relative (window, -1);
}

static void
action_tab_switch_right_cb (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  tab_switch_relative (window, 1);
}

static void
action_tab_move_left_cb (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  int change;

  if (priv->active_screen == nullptr)
    return;

  change = gtk_widget_get_direction (GTK_WIDGET (window)) == GTK_TEXT_DIR_RTL ? 1 : -1;
  terminal_mdi_container_reorder_screen (priv->mdi_container,
                                         terminal_mdi_container_get_active_screen (priv->mdi_container),
                                         change);
}

static void
action_tab_move_right_cb (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  int change;

  if (priv->active_screen == nullptr)
    return;

  change = gtk_widget_get_direction (GTK_WIDGET (window)) == GTK_TEXT_DIR_RTL ? -1 : 1;
  terminal_mdi_container_reorder_screen (priv->mdi_container,
                                         terminal_mdi_container_get_active_screen (priv->mdi_container),
                                         change);
}

static void
action_zoom_in_cb (GSimpleAction *action,
                   GVariant *parameter,
                   gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  double zoom;

  if (priv->active_screen == nullptr)
    return;

  zoom = vte_terminal_get_font_scale (VTE_TERMINAL (priv->active_screen));
  if (!find_larger_zoom_factor (&zoom))
    return;

  vte_terminal_set_font_scale (VTE_TERMINAL (priv->active_screen), zoom);
  terminal_window_update_zoom_sensitivity (window);
}

static void
action_zoom_out_cb (GSimpleAction *action,
                    GVariant *parameter,
                    gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  double zoom;

  if (priv->active_screen == nullptr)
    return;

  zoom = vte_terminal_get_font_scale (VTE_TERMINAL (priv->active_screen));
  if (!find_smaller_zoom_factor (&zoom))
    return;

  vte_terminal_set_font_scale (VTE_TERMINAL (priv->active_screen), zoom);
  terminal_window_update_zoom_sensitivity (window);
}

static void
action_zoom_normal_cb (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == nullptr)
    return;

  vte_terminal_set_font_scale (VTE_TERMINAL (priv->active_screen), PANGO_SCALE_MEDIUM);
  terminal_window_update_zoom_sensitivity (window);
}

static void
action_tab_detach_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  TerminalWindow *new_window;
  TerminalScreen *screen;
  char geometry[32];
  int width, height;

  app = terminal_app_get ();

  screen = priv->active_screen;

  terminal_screen_get_size (screen, &width, &height);
  g_snprintf (geometry, sizeof (geometry), "%dx%d", width, height);

  new_window = terminal_window_new (G_APPLICATION (app));

  terminal_window_move_screen (window, new_window, screen, -1);

  terminal_window_parse_geometry (new_window, geometry);

  gtk_window_present_with_time (GTK_WINDOW (new_window), gtk_get_current_event_time ());
}

static void
action_help_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  terminal_util_show_help (nullptr);
}

static void
action_about_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  terminal_util_show_about ();
}

static void
action_edit_preferences_cb (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  terminal_app_edit_preferences (terminal_app_get (),
                                 terminal_screen_get_profile (priv->active_screen),
                                 nullptr,
                                 gtk_get_current_event_time());
}

static void
action_size_to_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  guint width, height;

  g_assert_nonnull (parameter);

  if (priv->active_screen == nullptr)
    return;

  g_variant_get (parameter, "(uu)", &width, &height);
  if (width < MIN_WIDTH_CHARS || height < MIN_HEIGHT_CHARS ||
      width > 256 || height > 256)
    return;

  vte_terminal_set_size (VTE_TERMINAL (priv->active_screen), width, height);
  terminal_window_update_size (window);
}

static void
action_open_match_cb (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;

  if (info == nullptr)
    return;
  if (info->url == nullptr)
    return;

  terminal_util_open_url (GTK_WIDGET (window), info->url, info->url_flavor,
                          gtk_get_current_event_time ());
}

static void
action_copy_match_cb (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;

  if (info == nullptr)
    return;
  if (info->url == nullptr)
    return;

  gtk_clipboard_set_text (priv->clipboard, info->url, -1);
}

static void
action_open_hyperlink_cb (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;

  if (info == nullptr)
    return;
  if (info->hyperlink == nullptr)
    return;

  terminal_util_open_url (GTK_WIDGET (window), info->hyperlink, FLAVOR_AS_IS,
                          gtk_get_current_event_time ());
}

static void
action_copy_hyperlink_cb (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;

  if (info == nullptr)
    return;
  if (info->hyperlink == nullptr)
    return;

  gtk_clipboard_set_text (priv->clipboard, info->hyperlink, -1);
}

static void
action_enter_fullscreen_cb (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  g_action_group_change_action_state (G_ACTION_GROUP (window), "fullscreen",
                                      g_variant_new_boolean (TRUE));
}

static void
action_leave_fullscreen_cb (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  g_action_group_change_action_state (G_ACTION_GROUP (window), "fullscreen",
                                      g_variant_new_boolean (FALSE));
}

static void
action_inspector_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
search_popover_search_cb (TerminalSearchPopover *popover,
                          gboolean backward,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (G_UNLIKELY (priv->active_screen == nullptr))
    return;

  if (backward)
    vte_terminal_search_find_previous (VTE_TERMINAL (priv->active_screen));
  else
    vte_terminal_search_find_next (VTE_TERMINAL (priv->active_screen));
}

static void
search_popover_notify_regex_cb (TerminalSearchPopover *popover,
                                GParamSpec *pspec G_GNUC_UNUSED,
                                TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  VteRegex *regex;

  if (G_UNLIKELY (priv->active_screen == nullptr))
    return;

  regex = terminal_search_popover_get_regex (popover);
  vte_terminal_search_set_regex (VTE_TERMINAL (priv->active_screen), regex, 0);

  terminal_window_update_search_sensitivity (priv->active_screen, window);
}

static void
search_popover_notify_wrap_around_cb (TerminalSearchPopover *popover,
                                      GParamSpec *pspec G_GNUC_UNUSED,
                                      TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  gboolean wrap;

  if (G_UNLIKELY (priv->active_screen == nullptr))
    return;

  wrap = terminal_search_popover_get_wrap_around (popover);
  vte_terminal_search_set_wrap_around (VTE_TERMINAL (priv->active_screen), wrap);
}

static void
action_find_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (G_UNLIKELY(priv->active_screen == nullptr))
    return;

  if (priv->search_popover != nullptr) {
    search_popover_notify_regex_cb (priv->search_popover, nullptr, window);
    search_popover_notify_wrap_around_cb (priv->search_popover, nullptr, window);

    gtk_window_present_with_time (GTK_WINDOW (priv->search_popover),
                                  gtk_get_current_event_time ());
    gtk_widget_grab_focus (GTK_WIDGET (priv->search_popover));
    return;
  }

  if (priv->active_screen == nullptr)
    return;

  priv->search_popover = terminal_search_popover_new (GTK_WIDGET (window));

  g_signal_connect (priv->search_popover, "search", G_CALLBACK (search_popover_search_cb), window);

  search_popover_notify_regex_cb (priv->search_popover, nullptr, window);
  g_signal_connect (priv->search_popover, "notify::regex", G_CALLBACK (search_popover_notify_regex_cb), window);

  search_popover_notify_wrap_around_cb (priv->search_popover, nullptr, window);
  g_signal_connect (priv->search_popover, "notify::wrap-around", G_CALLBACK (search_popover_notify_wrap_around_cb), window);

  g_signal_connect (priv->search_popover, "destroy", G_CALLBACK (gtk_widget_destroyed), &priv->search_popover);

  gtk_window_present_with_time (GTK_WINDOW (priv->search_popover), gtk_get_current_event_time ());
  gtk_widget_grab_focus (GTK_WIDGET (priv->search_popover));
}

static void
action_find_forward_cb (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == nullptr)
    return;

  vte_terminal_search_find_next (VTE_TERMINAL (priv->active_screen));
}

static void
action_find_backward_cb (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == nullptr)
    return;

  vte_terminal_search_find_previous (VTE_TERMINAL (priv->active_screen));
}

static void
action_find_clear_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == nullptr)
    return;

  vte_terminal_search_set_regex (VTE_TERMINAL (priv->active_screen), nullptr, 0);
  vte_terminal_unselect_all (VTE_TERMINAL (priv->active_screen));
}

static void
action_shadow_activate_cb (GSimpleAction *action,
                           GVariant *parameter,
                           gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  gs_free char *param = g_variant_print(parameter, TRUE);

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "Window %p shadow action activated for %s\n",
                         window, param);

  /* We make sure in terminal-accels to always install the keybinding
   * for the real action first, so that it's first in line for activation.
   * That means we can make this here a NOP, instead of forwarding the
   * activation to the shadowed action.
   */
}

static void
action_menubar_visible_state_cb (GSimpleAction *action,
                                 GVariant *state,
                                 gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  gboolean active;

  active = g_variant_get_boolean (state);
  terminal_window_set_menubar_visible (window, active); /* this also sets the action state */
}

static void
action_fullscreen_state_cb (GSimpleAction *action,
                            GVariant *state,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (g_variant_get_boolean (state))
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));

  /* The window-state-changed callback will update the action's actual state */
}

static void
action_read_only_state_cb (GSimpleAction *action,
                           GVariant *state,
                           gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  g_assert_nonnull (state);

  g_simple_action_set_state (action, state);

  terminal_window_update_paste_sensitivity (window);

  if (priv->active_screen == nullptr)
    return;

  vte_terminal_set_input_enabled (VTE_TERMINAL (priv->active_screen),
                                  !g_variant_get_boolean (state));
}

static void
action_profile_state_cb (GSimpleAction *action,
                         GVariant *state,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalSettingsList *profiles_list;
  const gchar *uuid;
  gs_unref_object GSettings *profile;

  g_assert_nonnull (state);

  uuid = g_variant_get_string (state, nullptr);
  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  profile = terminal_settings_list_ref_child (profiles_list, uuid);
  if (profile == nullptr)
    return;

  g_simple_action_set_state (action, state);

  terminal_screen_set_profile (priv->active_screen, profile);
}

static void
action_active_tab_set_cb (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;
  int value, n_screens;

  g_assert_nonnull (parameter);

  n_screens = terminal_mdi_container_get_n_screens (priv->mdi_container);

  value = g_variant_get_int32 (parameter);
  if (value < 0)
    value += n_screens;
  if (value < 0 || value >= n_screens)
    return;

  g_action_change_state (G_ACTION (action), g_variant_new_int32 (value));
}

static void
action_active_tab_state_cb (GSimpleAction *action,
                            GVariant *state,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalWindowPrivate *priv = window->priv;

  g_assert_nonnull (state);

  g_simple_action_set_state (action, state);

  terminal_mdi_container_set_active_screen_num (priv->mdi_container, g_variant_get_int32 (state));
}

/* Menubar mnemonics & accel settings handling */

static void
enable_menubar_accel_changed_cb (GSettings *settings,
                                 const char *key,
                                 GtkSettings *gtk_settings)
{
  if (g_settings_get_boolean (settings, key))
    gtk_settings_reset_property (gtk_settings, "gtk-menu-bar-accel");
  else
    g_object_set (gtk_settings, "gtk-menu-bar-accel", nullptr, nullptr);
}

/* The menubar is shown by the app, and the use of mnemonics (e.g. Alt+F for File) is toggled.
 * The mnemonic modifier is per window, so it doesn't affect the Find or Preferences windows.
 * If the menubar is shown by the shell, a non-mnemonic variant of the menu is loaded instead
 * in terminal-app.c. See over there for further details. */
static void
enable_mnemonics_changed_cb (GSettings *settings,
                             const char *key,
                             TerminalWindow *window)
{
  gboolean enabled = g_settings_get_boolean (settings, key);

  if (enabled)
    gtk_window_set_mnemonic_modifier (GTK_WINDOW (window), GDK_MOD1_MASK);
  else
    gtk_window_set_mnemonic_modifier (GTK_WINDOW (window), GdkModifierType(GDK_MODIFIER_MASK & ~GDK_RELEASE_MASK));
}

static void
app_setting_notify_destroy_cb (GtkSettings *gtk_settings)
{
  g_signal_handlers_disconnect_by_func (terminal_app_get_global_settings (terminal_app_get ()),
                                        (void*)enable_menubar_accel_changed_cb,
                                        gtk_settings);
}

/* utility functions */

static int
find_tab_num_at_pos (GtkNotebook *notebook,
                     int screen_x, 
                     int screen_y)
{
  GtkPositionType tab_pos;
  int page_num = 0;
  GtkNotebook *nb = GTK_NOTEBOOK (notebook);
  GtkWidget *page;
  GtkAllocation tab_allocation;

  tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

  while ((page = gtk_notebook_get_nth_page (nb, page_num)))
    {
      GtkWidget *tab;
      int max_x, max_y, x_root, y_root;

      tab = gtk_notebook_get_tab_label (nb, page);
      g_return_val_if_fail (tab != nullptr, -1);

      if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
        {
          page_num++;
          continue;
        }

      gdk_window_get_origin (gtk_widget_get_window (tab), &x_root, &y_root);

      gtk_widget_get_allocation (tab, &tab_allocation);
      max_x = x_root + tab_allocation.x + tab_allocation.width;
      max_y = y_root + tab_allocation.y + tab_allocation.height;

      if ((tab_pos == GTK_POS_TOP || tab_pos == GTK_POS_BOTTOM) && screen_x <= max_x)
        return page_num;

      if ((tab_pos == GTK_POS_LEFT || tab_pos == GTK_POS_RIGHT) && screen_y <= max_y)
        return page_num;

      page_num++;
    }

  return -1;
}

static void
terminal_window_update_set_profile_menu_active_profile (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GSettings *new_active_profile;
  TerminalSettingsList *profiles_list;
  char *uuid;

  if (priv->active_screen == nullptr)
    return;

  new_active_profile = terminal_screen_get_profile (priv->active_screen);

  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  uuid = terminal_settings_list_dup_uuid_from_child (profiles_list, new_active_profile);

  g_simple_action_set_state (lookup_action (window, "profile"),
                             g_variant_new_take_string (uuid));
}

static void
terminal_window_update_terminal_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == nullptr)
    return;

  gboolean read_only = !vte_terminal_get_input_enabled (VTE_TERMINAL (priv->active_screen));
  g_simple_action_set_state (lookup_action (window, "read-only"),
                             g_variant_new_boolean (read_only));
}

/* Actions stuff */

static void
terminal_window_update_copy_sensitivity (TerminalScreen *screen,
                                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  gboolean can_copy;

  if (screen != priv->active_screen)
    return;

  can_copy = vte_terminal_get_has_selection (VTE_TERMINAL (screen));
  g_simple_action_set_enabled (lookup_action (window, "copy"), can_copy);
}

static void
terminal_window_update_zoom_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *screen;

  screen = priv->active_screen;
  if (screen == nullptr)
    return;

  double v;
  double zoom = v = vte_terminal_get_font_scale (VTE_TERMINAL (screen));
  g_simple_action_set_enabled (lookup_action (window, "zoom-in"),
                               find_larger_zoom_factor (&v));

  v = zoom;
  g_simple_action_set_enabled (lookup_action (window, "zoom-out"),
                               find_smaller_zoom_factor (&v));
}

static void
terminal_window_update_search_sensitivity (TerminalScreen *screen,
                                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (screen != priv->active_screen)
    return;

  gboolean can_search = vte_terminal_search_get_regex (VTE_TERMINAL (screen)) != nullptr;

  g_simple_action_set_enabled (lookup_action (window, "find-forward"), can_search);
  g_simple_action_set_enabled (lookup_action (window, "find-backward"), can_search);
  g_simple_action_set_enabled (lookup_action (window, "find-clear"), can_search);
}

static void
clipboard_targets_changed_cb (TerminalApp *app,
                              GtkClipboard *clipboard,
                              TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (clipboard != priv->clipboard)
    return;

  terminal_window_update_paste_sensitivity (window);
}

static void
terminal_window_update_paste_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  GdkAtom *targets;
  int n_targets;
  targets = terminal_app_get_clipboard_targets (terminal_app_get(), priv->clipboard, &n_targets);

  gboolean can_paste;
  gboolean can_paste_uris;
  if (n_targets) {
    can_paste = gtk_targets_include_text (targets, n_targets);
    can_paste_uris = gtk_targets_include_uri (targets, n_targets);
  } else {
    can_paste = can_paste_uris = FALSE;
  }

  gs_unref_variant GVariant *ro_state = g_action_get_state (g_action_map_lookup_action (G_ACTION_MAP (window), "read-only"));
  gboolean read_only = g_variant_get_boolean (ro_state);

  g_simple_action_set_enabled (lookup_action (window, "paste-text"), can_paste && !read_only);
  g_simple_action_set_enabled (lookup_action (window, "paste-uris"), can_paste_uris && !read_only);
}

static void
screen_resize_window_cb (TerminalScreen *screen,
                         guint columns,
                         guint rows,
                         TerminalWindow* window)
{
  auto const priv = window->priv;

  if (priv->realized &&
      window_state_is_snapped(priv->window_state))
    return;

  vte_terminal_set_size (VTE_TERMINAL (priv->active_screen), columns, rows);

  if (screen == priv->active_screen)
    terminal_window_update_size (window);
}

static void
terminal_window_update_tabs_actions_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->disposed)
    return;

  int num_pages = terminal_mdi_container_get_n_screens (priv->mdi_container);
  int page_num = terminal_mdi_container_get_active_screen_num (priv->mdi_container);

  gboolean not_only = num_pages > 1;
  gboolean not_first = page_num > 0;
  gboolean not_last = page_num + 1 < num_pages;

  gboolean not_first_lr, not_last_lr;
  if (gtk_widget_get_direction (GTK_WIDGET (window)) == GTK_TEXT_DIR_RTL) {
    not_first_lr = not_last;
    not_last_lr = not_first;
  } else {
    not_first_lr = not_first;
    not_last_lr = not_last;
  }

  /* Hide the tabs menu in single-tab windows */
  g_simple_action_set_enabled (lookup_action (window, "tabs-menu"), not_only);

  /* Disable shadowing of MDI actions in SDI windows */
  g_simple_action_set_enabled (lookup_action (window, "shadow-mdi"), not_only);

  /* Disable tab switching (and all its shortcuts) in SDI windows */
  g_simple_action_set_enabled (lookup_action (window, "active-tab"), not_only);

  /* Set the active tab */
  g_simple_action_set_state (lookup_action (window, "active-tab"),
                             g_variant_new_int32 (page_num));

  /* Keynav wraps around? See bug #92139 */
  gboolean keynav_wrap_around;
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                "gtk-keynav-wrap-around", &keynav_wrap_around,
                nullptr);

  gboolean wrap = keynav_wrap_around && not_only;
  g_simple_action_set_enabled (lookup_action (window, "tab-switch-left"), not_first || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-switch-right"), not_last || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-move-left"), not_first_lr || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-move-right"), not_last_lr || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-detach"), not_only);
}

static GtkNotebook *
handle_tab_droped_on_desktop (GtkNotebook *source_notebook,
                              GtkWidget   *container,
                              gint         x,
                              gint         y,
                              gpointer     data G_GNUC_UNUSED)
{
  TerminalWindow *source_window;
  TerminalWindow *new_window;
  TerminalWindowPrivate *new_priv;

  source_window = TERMINAL_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (source_notebook)));
  g_return_val_if_fail (TERMINAL_IS_WINDOW (source_window), nullptr);

  new_window = terminal_window_new (G_APPLICATION (terminal_app_get ()));
  new_priv = new_window->priv;
  new_priv->present_on_insert = TRUE;

  return GTK_NOTEBOOK (new_priv->mdi_container);
}

/* Terminal screen popup menu handling */

static void
remove_popup_info (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->popup_info != nullptr)
    {
      terminal_screen_popup_info_unref (priv->popup_info);
      priv->popup_info = nullptr;
    }
}

static void
screen_popup_menu_selection_done_cb (GtkWidget *popup,
                                     GtkWidget *window)
{
  g_signal_handlers_disconnect_by_func
    (popup, (void*)screen_popup_menu_selection_done_cb, window);

  GtkWidget *attach_widget = gtk_menu_get_attach_widget (GTK_MENU (popup));
  if (attach_widget != window || !TERMINAL_IS_WINDOW (attach_widget))
    return;

  remove_popup_info (TERMINAL_WINDOW (attach_widget));
}

static void
screen_show_popup_menu_cb (TerminalScreen *screen,
                           TerminalScreenPopupInfo *info,
                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app = terminal_app_get ();

  if (screen != priv->active_screen)
    return;

  remove_popup_info (window);
  priv->popup_info = terminal_screen_popup_info_ref (info);

  gs_unref_object GMenu *menu = g_menu_new ();

  /* Hyperlink section */
  if (info->hyperlink != nullptr) {
    gs_unref_object GMenu *section1 = g_menu_new ();

    g_menu_append (section1, _("Open _Hyperlink"), "win.open-hyperlink");
    g_menu_append (section1, _("Copy Hyperlink _Address"), "win.copy-hyperlink");
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section1));
  }
  /* Matched link section */
  else if (info->url != nullptr) {
    gs_unref_object GMenu *section2 = g_menu_new ();

    const char *open_label = nullptr, *copy_label = nullptr;
    switch (info->url_flavor) {
    case FLAVOR_EMAIL:
      open_label = _("Send Mail _To…");
      copy_label = _("Copy Mail _Address");
      break;
    case FLAVOR_VOIP_CALL:
      open_label = _("Call _To…");
      copy_label = _("Copy Call _Address ");
      break;
    case FLAVOR_AS_IS:
    case FLAVOR_DEFAULT_TO_HTTP:
    default:
      open_label = _("_Open Link");
      copy_label = _("Copy _Link");
      break;
    }

    g_menu_append (section2, open_label, "win.open-match");
    g_menu_append (section2, copy_label, "win.copy-match");
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section2));
  }

  /* Info section */
  gs_strfreev char** citems = g_settings_get_strv (terminal_app_get_global_settings (terminal_app_get ()),
                                                   TERMINAL_SETTING_CONTEXT_INFO_KEY);

  gs_unref_object GMenu *section3 = g_menu_new ();

  for (int i = 0; citems[i] != nullptr; ++i) {
    const char *citem = citems[i];

    if (g_str_equal (citem, "numbers") &&
        info->number_info != nullptr) {
      /* Non-existent action will make this item insensitive */
      gs_unref_object GMenuItem *item3 = g_menu_item_new (info->number_info, "win.notexist");
      g_menu_append_item (section3, item3);
    }

    if (g_str_equal (citem, "timestamps") &&
        info->timestamp_info != nullptr) {
      /* Non-existent action will make this item insensitive */
      gs_unref_object GMenuItem *item3 = g_menu_item_new (info->timestamp_info, "win.notexist");
      g_menu_append_item (section3, item3);
    }
  }

  if (g_menu_model_get_n_items(G_MENU_MODEL (section3)) > 0)
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section3));

  /* Clipboard section */
  gs_unref_object GMenu *section4 = g_menu_new ();

  g_menu_append (section4, _("_Copy"), "win.copy::text");
  g_menu_append (section4, _("Copy as _HTML"), "win.copy::html");
  g_menu_append (section4, _("_Paste"), "win.paste-text");
  if (g_action_get_enabled (G_ACTION (lookup_action (window, "paste-uris"))))
    g_menu_append (section4, _("Paste as _Filenames"), "win.paste-uris");

  g_menu_append_section (menu, nullptr, G_MENU_MODEL (section4));

  /* Profile and property section */
  gs_unref_object GMenu *section5 = g_menu_new ();
  g_menu_append (section5, _("Read-_Only"), "win.read-only");

  GMenuModel *profiles_menu = terminal_app_get_profile_section (app);
  if (profiles_menu != nullptr && g_menu_model_get_n_items (profiles_menu) > 1) {
    gs_unref_object GMenu *submenu5 = g_menu_new ();
    g_menu_append_section (submenu5, nullptr, profiles_menu);

    gs_unref_object GMenuItem *item5 = g_menu_item_new (_("P_rofiles"), nullptr);
    g_menu_item_set_submenu (item5, G_MENU_MODEL (submenu5));
    g_menu_append_item (section5, item5);
  }

  g_menu_append (section5, _("_Preferences"), "win.edit-preferences");

  g_menu_append_section (menu, nullptr, G_MENU_MODEL (section5));

  /* New Terminal section */
  gs_unref_object GMenu *section6 = g_menu_new ();
  if (terminal_app_get_menu_unified (app)) {
    gs_unref_object GMenuItem *item6 = g_menu_item_new (_("New _Terminal"), nullptr);
    g_menu_item_set_action_and_target (item6, "win.new-terminal",
                                       "(ss)", "default", "current");
    g_menu_append_item (section6, item6);
  } else {
    gs_unref_object GMenuItem *item61 = g_menu_item_new (_("New _Window"), nullptr);
    g_menu_item_set_action_and_target (item61, "win.new-terminal",
                                       "(ss)", "window", "current");
    g_menu_append_item (section6, item61);
    gs_unref_object GMenuItem *item62 = g_menu_item_new (_("New _Tab"), nullptr);
    g_menu_item_set_action_and_target (item62, "win.new-terminal",
                                       "(ss)", "tab", "current");
    g_menu_append_item (section6, item62);
  }
  g_menu_append_section (menu, nullptr, G_MENU_MODEL (section6));

  /* Window section */
  gs_unref_object GMenu *section7 = g_menu_new ();

  /* Only show this if the WM doesn't show the menubar */
  if (g_action_get_enabled (G_ACTION (lookup_action (window, "menubar-visible"))))
    g_menu_append (section7, _("Show _Menubar"), "win.menubar-visible");
  if (g_action_get_enabled (G_ACTION (lookup_action (window, "leave-fullscreen"))))
    g_menu_append (section7, _("L_eave Full Screen"), "win.leave-fullscreen");

  g_menu_append_section (menu, nullptr, G_MENU_MODEL (section7));

  /* Now create the popup menu and show it */
  GtkWidget *popup_menu = context_menu_new (G_MENU_MODEL (menu), GTK_WIDGET (window));

  /* Remove the popup info after the menu is done */
  g_signal_connect (popup_menu, "selection-done",
                    G_CALLBACK (screen_popup_menu_selection_done_cb), window);

  gtk_menu_popup (GTK_MENU (popup_menu), nullptr, nullptr,
                  nullptr, nullptr,
                  info->button,
                  info->timestamp);

  if (info->button == 0)
    gtk_menu_shell_select_first (GTK_MENU_SHELL (popup_menu), FALSE);
}

static gboolean
screen_match_clicked_cb (TerminalScreen *screen,
                         const char *url,
                         int url_flavor,
                         guint state,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (screen != priv->active_screen)
    return FALSE;

  gtk_widget_grab_focus (GTK_WIDGET (screen));
  terminal_util_open_url (GTK_WIDGET (window), url, TerminalURLFlavor(url_flavor),
                          gtk_get_current_event_time ());

  return TRUE;
}

static void
screen_close_cb (TerminalScreen *screen,
                 TerminalWindow *window)
{
  terminal_window_remove_screen (window, screen);
}

static void
notebook_update_tabs_menu_cb (GtkMenuButton *button,
                              TerminalWindow *window)
{
  gs_unref_object GMenu *menu;
  gs_free_list GList *tabs;
  GList *t;
  int i;

  menu = g_menu_new ();
  tabs = terminal_window_list_screen_containers (window);

  for (t = tabs, i = 0; t != nullptr; t = t->next, i++) {
    TerminalScreenContainer *container = (TerminalScreenContainer*)t->data;
    TerminalScreen *screen = terminal_screen_container_get_screen (container);
    gs_unref_object GMenuItem *item;
    const char *title;

    if (t->next == nullptr) {
      /* Last entry. If it has no dedicated shortcut "Switch to Tab N",
       * display the accel of "Switch to Last Tab". */
      GtkApplication *app = GTK_APPLICATION (g_application_get_default ());
      gs_free gchar *detailed_action = g_strdup_printf("win.active-tab(%d)", i);
      gs_strfreev gchar **accels = gtk_application_get_accels_for_action (app, detailed_action);
      if (accels[0] == nullptr)
        i = -1;
    }

    title = terminal_screen_get_title (screen);

    item = g_menu_item_new (title && title[0] ? title : _("Terminal"), nullptr);
    g_menu_item_set_action_and_target (item, "win.active-tab", "i", i);
    g_menu_append_item (menu, item);
  }

  gtk_menu_button_set_menu_model (button, G_MENU_MODEL (menu));

  /* Need this so the menu is positioned correctly */
  gtk_widget_set_halign (GTK_WIDGET (gtk_menu_button_get_popup (button)), GTK_ALIGN_END);
}

static void
terminal_window_fill_notebook_action_box (TerminalWindow *window,
                                          gboolean add_new_tab_button)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *box, *new_tab_button, *tabs_menu_button;

  box = terminal_notebook_get_action_box (TERMINAL_NOTEBOOK (priv->mdi_container), GTK_PACK_END);

  /* Create the NewTerminal button */
  if (add_new_tab_button)
    {
      new_tab_button = terminal_icon_button_new ("tab-new-symbolic");
      gtk_actionable_set_action_name (GTK_ACTIONABLE (new_tab_button), "win.new-terminal");
      gtk_actionable_set_action_target (GTK_ACTIONABLE (new_tab_button), "(ss)", "tab", "current");
      gtk_box_pack_start (GTK_BOX (box), new_tab_button, FALSE, FALSE, 0);
      gtk_widget_show (new_tab_button);
    }

  /* Create Tabs menu button */
  tabs_menu_button = terminal_menu_button_new ();
  g_signal_connect (tabs_menu_button, "update-menu",
                    G_CALLBACK (notebook_update_tabs_menu_cb), window);
  gtk_box_pack_start (GTK_BOX (box), tabs_menu_button, FALSE, FALSE, 0);
  gtk_menu_button_set_align_widget (GTK_MENU_BUTTON (tabs_menu_button), box);
  gtk_widget_show (tabs_menu_button);
}

static void
window_hide_ask_default_terminal(TerminalWindow* window)
{
  auto const priv = window->priv;

  if (!priv->ask_default_infobar ||
      !gtk_widget_get_visible(priv->ask_default_infobar))
    return;

  gtk_widget_hide(priv->ask_default_infobar);
  terminal_window_update_size(window);
}

static void
window_sync_ask_default_terminal_cb(TerminalApp* app,
                                    GParamSpec* pspect,
                                    TerminalWindow* window)
{
  window_hide_ask_default_terminal(window);
}

static void
default_infobar_response_cb(GtkInfoBar* infobar,
                            int response,
                            TerminalWindow* window)
{
  auto const app = terminal_app_get();

  if (response == GTK_RESPONSE_YES) {
    terminal_app_make_default_terminal(app);
    terminal_app_unset_ask_default_terminal(app);
  } else if (response == GTK_RESPONSE_NO) {
    terminal_app_unset_ask_default_terminal(app);
  } else { // GTK_RESPONSE_CLOSE
    window_hide_ask_default_terminal(window);
  }
}

/*****************************************/

static void
terminal_window_realize (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] realize, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->realize (widget);

  /* Now that we've been realized, we should know precisely how large the
   * client-side decorations are going to be. Recalculate the geometry hints,
   * export them to the windowing system, and resize the window accordingly. */
  priv->realized = TRUE;
  terminal_window_update_size (window);
}

static gboolean
terminal_window_state_event (GtkWidget            *widget,
                             GdkEventWindowState  *event)
{
  auto const window_state_event =
    GTK_WIDGET_CLASS(terminal_window_parent_class)->window_state_event;
  auto const window = TERMINAL_WINDOW(widget);
  auto const priv = window->priv;

  priv->window_state = event->new_window_state;

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "Window state changed mask %x old state %x new state %x\n",
                         unsigned(event->changed_mask),
                         unsigned(priv->window_state),
                         unsigned(event->new_window_state));

  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      auto const is_fullscreen = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;

      g_simple_action_set_state (lookup_action (window, "fullscreen"),
                                 g_variant_new_boolean (is_fullscreen));
      g_simple_action_set_enabled (lookup_action (window, "leave-fullscreen"),
                                   is_fullscreen);
      g_simple_action_set_enabled (lookup_action (window, "size-to"),
                                   !is_fullscreen);
    }

  if (window_state_is_snapped(event->changed_mask))
    {
      if (window_state_is_snapped(event->new_window_state))
        {
          _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                                 "Disapplying geometry hints entering snapped state\n");
          gtk_window_set_geometry_hints(GTK_WINDOW(widget), nullptr, nullptr,
                                        GdkWindowHints(0));
        }
      else
        {
          _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                                 "Reapplying geometry hints after leaving snapped state\n");
          gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                         nullptr,
                                         &priv->hints,
                                         GdkWindowHints(GDK_HINT_RESIZE_INC |
                                                        GDK_HINT_MIN_SIZE |
                                                        GDK_HINT_BASE_SIZE));
        }
    }

  if (window_state_event)
    return window_state_event (widget, event);

  return false;
}

static void
terminal_window_screen_update (TerminalWindow *window,
                               GdkScreen *screen)
{
  GSettings *settings;
  GtkSettings *gtk_settings;

  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (screen), "GT::HasSettingsConnection")))
    return;

  settings = terminal_app_get_global_settings (terminal_app_get ());
  gtk_settings = gtk_settings_get_for_screen (screen);

  g_object_set_data_full (G_OBJECT (screen), "GT::HasSettingsConnection",
                          gtk_settings,
                          (GDestroyNotify) app_setting_notify_destroy_cb);

  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_SHORTCUTS_KEY,
                   gtk_settings,
                   "gtk-enable-accels",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET));

  enable_menubar_accel_changed_cb (settings,
                                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                                   gtk_settings);
  g_signal_connect (settings, "changed::" TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                    G_CALLBACK (enable_menubar_accel_changed_cb),
                    gtk_settings);
}

static void
terminal_window_screen_changed (GtkWidget *widget,
                                GdkScreen *previous_screen)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  void (* screen_changed) (GtkWidget *, GdkScreen *) =
    GTK_WIDGET_CLASS (terminal_window_parent_class)->screen_changed;
  GdkScreen *screen;

  if (screen_changed)
    screen_changed (widget, previous_screen);

  screen = gtk_widget_get_screen (widget);
  if (previous_screen == screen)
    return;

  if (!screen)
    return;

  terminal_window_screen_update (window, screen);
}

static void
terminal_window_init (TerminalWindow *window)
{
  const GActionEntry action_entries[] = {
    /* Actions without state */
    { "about",               action_about_cb,            nullptr,   nullptr, nullptr },
    { "close",               action_close_cb,            "s",    nullptr, nullptr },
    { "copy",                action_copy_cb,             "s",    nullptr, nullptr },
    { "copy-hyperlink",      action_copy_hyperlink_cb,   nullptr,   nullptr, nullptr },
    { "copy-match",          action_copy_match_cb,       nullptr,   nullptr, nullptr },
    { "edit-preferences",    action_edit_preferences_cb, nullptr,   nullptr, nullptr },
    { "enter-fullscreen",    action_enter_fullscreen_cb, nullptr,   nullptr, nullptr },
    { "find",                action_find_cb,             nullptr,   nullptr, nullptr },
    { "find-backward",       action_find_backward_cb,    nullptr,   nullptr, nullptr },
    { "find-clear",          action_find_clear_cb,       nullptr,   nullptr, nullptr },
    { "find-forward",        action_find_forward_cb,     nullptr,   nullptr, nullptr },
    { "help",                action_help_cb,             nullptr,   nullptr, nullptr },
    { "inspector",           action_inspector_cb,        nullptr,   nullptr, nullptr },
    { "leave-fullscreen",    action_leave_fullscreen_cb, nullptr,   nullptr, nullptr },
    { "new-terminal",        action_new_terminal_cb,     "(ss)", nullptr, nullptr },
    { "open-match",          action_open_match_cb,       nullptr,   nullptr, nullptr },
    { "open-hyperlink",      action_open_hyperlink_cb,   nullptr,   nullptr, nullptr },
    { "paste-text",          action_paste_text_cb,       nullptr,   nullptr, nullptr },
    { "paste-uris",          action_paste_uris_cb,       nullptr,   nullptr, nullptr },
    { "reset",               action_reset_cb,            "b",    nullptr, nullptr },
    { "select-all",          action_select_all_cb,       nullptr,   nullptr, nullptr },
    { "size-to",             action_size_to_cb,          "(uu)", nullptr, nullptr },
    { "tab-detach",          action_tab_detach_cb,       nullptr,   nullptr, nullptr },
    { "tab-move-left",       action_tab_move_left_cb,    nullptr,   nullptr, nullptr },
    { "tab-move-right",      action_tab_move_right_cb,   nullptr,   nullptr, nullptr },
    { "tab-switch-left",     action_tab_switch_left_cb,  nullptr,   nullptr, nullptr },
    { "tab-switch-right",    action_tab_switch_right_cb, nullptr,   nullptr, nullptr },
    { "tabs-menu",           nullptr,                       nullptr,   nullptr, nullptr },
    { "zoom-in",             action_zoom_in_cb,          nullptr,   nullptr, nullptr },
    { "zoom-normal",         action_zoom_normal_cb,      nullptr,   nullptr, nullptr },
    { "zoom-out",            action_zoom_out_cb,         nullptr,   nullptr, nullptr },
#ifdef ENABLE_EXPORT
    { "export",              action_export_cb,           nullptr,   nullptr, nullptr },
#endif
#ifdef ENABLE_PRINT
    { "print",               action_print_cb,            nullptr,   nullptr, nullptr },
#endif
#ifdef ENABLE_SAVE
    { "save-contents",       action_save_contents_cb,    nullptr,   nullptr, nullptr },
#endif

    /* Shadow actions for keybinding comsumption, see comment in terminal-accels.c */
    { "shadow",              action_shadow_activate_cb,  "s",    nullptr, nullptr },
    { "shadow-mdi",          action_shadow_activate_cb,  "s",    nullptr, nullptr },

    /* Actions with state */
    { "active-tab",          action_active_tab_set_cb,   "i",  "@i 0",    action_active_tab_state_cb      },
    { "header-menu",         nullptr /* toggles state */,   nullptr, "false",   nullptr },
    { "fullscreen",          nullptr /* toggles state */,   nullptr, "false",   action_fullscreen_state_cb      },
    { "menubar-visible",     nullptr /* toggles state */,   nullptr, "true",    action_menubar_visible_state_cb },
    { "profile",             nullptr /* changes state */,   "s",  "''",      action_profile_state_cb         },
    { "read-only",           nullptr /* toggles state */,   nullptr, "false",   action_read_only_state_cb       },
  };
  TerminalWindowPrivate *priv;
  TerminalApp *app;
  GSettings *gtk_debug_settings;
  GtkWindowGroup *window_group;
  //  GtkAccelGroup *accel_group;
  uuid_t u;
  char uuidstr[37], role[64];
  gboolean shell_shows_menubar;
  gboolean use_headerbar;
  GSimpleAction *action;

  app = terminal_app_get ();

  priv = window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, TERMINAL_TYPE_WINDOW, TerminalWindowPrivate);

  gtk_widget_init_template (GTK_WIDGET (window));

  uuid_generate (u);
  uuid_unparse (u, uuidstr);
  priv->uuid = g_strdup (uuidstr);

  g_signal_connect (G_OBJECT (window), "delete_event",
                    G_CALLBACK(terminal_window_delete_event),
                    nullptr);

  use_headerbar = terminal_app_get_use_headerbar (app);
  if (use_headerbar) {
    GtkWidget *headerbar;

    headerbar = terminal_headerbar_new ();
    gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);
  }

  gtk_window_set_title (GTK_WINDOW (window), _("Terminal"));

  priv->active_screen = nullptr;

  priv->main_vbox = gtk_bin_get_child (GTK_BIN (window));

  priv->mdi_container = TERMINAL_MDI_CONTAINER (terminal_notebook_new ());

  g_signal_connect (priv->mdi_container, "screen-close-request",
                    G_CALLBACK (screen_close_request_cb), window);

  g_signal_connect_after (priv->mdi_container, "screen-switched",
                          G_CALLBACK (mdi_screen_switched_cb), window);
  g_signal_connect_after (priv->mdi_container, "screen-added",
                          G_CALLBACK (mdi_screen_added_cb), window);
  g_signal_connect_after (priv->mdi_container, "screen-removed",
                          G_CALLBACK (mdi_screen_removed_cb), window);
  g_signal_connect_after (priv->mdi_container, "screens-reordered",
                          G_CALLBACK (mdi_screens_reordered_cb), window);

  g_signal_connect_swapped (priv->mdi_container, "notify::tab-pos",
                            G_CALLBACK (terminal_window_update_geometry), window);
  g_signal_connect_swapped (priv->mdi_container, "notify::show-tabs",
                            G_CALLBACK (terminal_window_update_geometry), window);

  /* FIXME hack hack! */
  if (GTK_IS_NOTEBOOK (priv->mdi_container)) {
    g_signal_connect (priv->mdi_container, "button-press-event",
                      G_CALLBACK (notebook_button_press_cb), window);
    g_signal_connect (priv->mdi_container, "popup-menu",
                      G_CALLBACK (notebook_popup_menu_cb), window);
    g_signal_connect (priv->mdi_container, "create-window",
                      G_CALLBACK (handle_tab_droped_on_desktop), window);
  }

  gtk_box_pack_end (GTK_BOX (priv->main_vbox), GTK_WIDGET (priv->mdi_container), TRUE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (priv->mdi_container));

  priv->old_char_width = -1;
  priv->old_char_height = -1;

  priv->old_chrome_width = -1;
  priv->old_chrome_height = -1;
  priv->old_csd_width = -1;
  priv->old_csd_height = -1;
  priv->old_padding_width = -1;
  priv->old_padding_height = -1;

  priv->old_geometry_widget = nullptr;

  /* GAction setup */
  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   action_entries, G_N_ELEMENTS (action_entries),
                                   window);

  g_simple_action_set_enabled (lookup_action (window, "leave-fullscreen"), FALSE);

  GSettings *global_settings = terminal_app_get_global_settings (app);
  enable_mnemonics_changed_cb (global_settings, TERMINAL_SETTING_ENABLE_MNEMONICS_KEY, window);
  g_signal_connect (global_settings, "changed::" TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                    G_CALLBACK (enable_mnemonics_changed_cb), window);

  /* Hide "menubar-visible" when the menubar is shown by the shell */
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                "gtk-shell-shows-menubar", &shell_shows_menubar,
                nullptr);
  if (shell_shows_menubar) {
    g_simple_action_set_enabled (lookup_action (window, "menubar-visible"), FALSE);
  } else {
    priv->menubar = gtk_menu_bar_new_from_model (terminal_app_get_menubar (app));
    gtk_box_pack_start (GTK_BOX (priv->main_vbox),
                        priv->menubar,
                        FALSE, FALSE, 0);

    terminal_window_set_menubar_visible (window, !use_headerbar);
    priv->use_default_menubar_visibility = !use_headerbar;
  }

  /* Add "Set as default terminal" infobar */
  if (terminal_app_get_ask_default_terminal(app)) {
    auto const infobar = priv->ask_default_infobar = gtk_info_bar_new();
    gtk_info_bar_set_show_close_button(GTK_INFO_BAR(infobar), true);
    gtk_info_bar_set_message_type(GTK_INFO_BAR(infobar), GTK_MESSAGE_QUESTION);

    auto const question = gtk_label_new (_("Set GNOME Terminal as your default terminal?"));
    gtk_label_set_line_wrap(GTK_LABEL(question), true);
    auto const box = gtk_info_bar_get_content_area(GTK_INFO_BAR(infobar));
    gtk_container_add(GTK_CONTAINER(box), question);
    gtk_widget_show(question);

    gtk_info_bar_add_button(GTK_INFO_BAR(infobar), _("_Yes"), GTK_RESPONSE_YES);
    gtk_info_bar_add_button(GTK_INFO_BAR(infobar), _("_No"), GTK_RESPONSE_NO);

    g_signal_connect (infobar, "response",
                      G_CALLBACK(default_infobar_response_cb), window);

    gtk_box_pack_start(GTK_BOX(priv->main_vbox), infobar, false, true, 0);

    gtk_widget_show(infobar);
    g_signal_connect(app, "notify::ask-default-terminal",
                     G_CALLBACK(window_sync_ask_default_terminal_cb), window);
  }

  /* Maybe make Inspector available */
  action = lookup_action (window, "inspector");
  gtk_debug_settings = terminal_app_get_gtk_debug_settings (app);
  if (gtk_debug_settings != nullptr)
    g_settings_bind (gtk_debug_settings,
                     "enable-inspector-keybinding",
                     action,
                     "enabled",
                     GSettingsBindFlags(G_SETTINGS_BIND_GET |
					G_SETTINGS_BIND_NO_SENSITIVITY));
  else
    g_simple_action_set_enabled (action, FALSE);

  priv->clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  clipboard_targets_changed_cb (app, priv->clipboard, window);
  g_signal_connect (app, "clipboard-targets-changed",
                    G_CALLBACK (clipboard_targets_changed_cb), window);

  terminal_window_fill_notebook_action_box (window, !use_headerbar);

  /* We have to explicitly call this, since screen-changed is NOT
   * emitted for the toplevel the first time!
   */
  terminal_window_screen_update (window, gtk_widget_get_screen (GTK_WIDGET (window)));

  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_unref (window_group);

  g_snprintf (role, sizeof (role), "gnome-terminal-window-%s", uuidstr);
  gtk_window_set_role (GTK_WINDOW (window), role);
}

static void
terminal_window_style_updated (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->style_updated (widget);

  terminal_window_update_size (window);
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = terminal_window_dispose;
  object_class->finalize = terminal_window_finalize;

  widget_class->show = terminal_window_show;
  widget_class->realize = terminal_window_realize;
  widget_class->window_state_event = terminal_window_state_event;
  widget_class->screen_changed = terminal_window_screen_changed;
  widget_class->style_updated = terminal_window_style_updated;

  GtkWindowClass *window_klass;
  GtkBindingSet *binding_set;

  window_klass = (GtkWindowClass*)g_type_class_ref (GTK_TYPE_WINDOW);
  binding_set = gtk_binding_set_by_class (window_klass);
  gtk_binding_entry_skip (binding_set, GDK_KEY_I, GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK));
  gtk_binding_entry_skip (binding_set, GDK_KEY_D, GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK));
  g_type_class_unref (window_klass);

  g_type_class_add_private (object_class, sizeof (TerminalWindowPrivate));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/window.ui");

  gtk_widget_class_set_css_name(widget_class, TERMINAL_WINDOW_CSS_NAME);
}

static void
terminal_window_dispose (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app = terminal_app_get ();

  if (!priv->disposed) {
    GSettings *global_settings = terminal_app_get_global_settings (app);
    g_signal_handlers_disconnect_by_func (global_settings,
                                          (void*)enable_mnemonics_changed_cb,
                                          window);
  }

  priv->disposed = TRUE;

  if (priv->clipboard != nullptr) {
    g_signal_handlers_disconnect_by_func (app,
                                          (void*)clipboard_targets_changed_cb,
                                          window);
    priv->clipboard = nullptr;
  }

  if (priv->ask_default_infobar) {
    g_signal_handlers_disconnect_by_func(app,
                                         (void*)window_sync_ask_default_terminal_cb,
                                         window);
    priv->ask_default_infobar = nullptr;
  }

  remove_popup_info (window);

  if (priv->search_popover != nullptr)
    {
      g_signal_handlers_disconnect_matched (priv->search_popover, G_SIGNAL_MATCH_DATA,
                                            0, 0, nullptr, nullptr, window);
      gtk_widget_destroy (GTK_WIDGET (priv->search_popover));
      priv->search_popover = nullptr;
    }

  G_OBJECT_CLASS (terminal_window_parent_class)->dispose (object);
}

static void
terminal_window_finalize (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;

  if (priv->confirm_close_dialog)
    gtk_dialog_response (GTK_DIALOG (priv->confirm_close_dialog),
                         GTK_RESPONSE_DELETE_EVENT);

  g_free (priv->uuid);

  G_OBJECT_CLASS (terminal_window_parent_class)->finalize (object);
}

static gboolean
terminal_window_delete_event (GtkWidget *widget,
                              GdkEvent *event,
                              gpointer data)
{
   return confirm_close_window_or_tab (TERMINAL_WINDOW (widget), nullptr);
}

static void
terminal_window_show (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] show, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  /* Because of the unexpected reentrancy caused by adding the tab to the notebook
   * showing the TerminalWindow, we can get here when the first page has been
   * added but not yet set current. By setting the page current, we get the
   * right size when we first show the window */
  if (GTK_IS_NOTEBOOK (priv->mdi_container) &&
      gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->mdi_container)) == -1)
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->mdi_container), 0);

  if (priv->active_screen != nullptr)
    {
      /* At this point, we have our GdkScreen, and hence the right
       * font size, so we can go ahead and size the window. */
      terminal_window_update_size (window);
    }

  GTK_WIDGET_CLASS (terminal_window_parent_class)->show (widget);
}

TerminalWindow*
terminal_window_new (GApplication *app)
{
  return reinterpret_cast<TerminalWindow*>
    (g_object_new (TERMINAL_TYPE_WINDOW,
		   "application", app,
		   "show-menubar", FALSE,
		   nullptr));
}

static void
profile_set_cb (TerminalScreen *screen,
                GSettings *old_profile,
                TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  terminal_window_update_set_profile_menu_active_profile (window);
}

static void
sync_screen_title (TerminalScreen *screen,
                   GParamSpec *psepc,
                   TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  const char *title;

  if (screen != priv->active_screen)
    return;

  title = terminal_screen_get_title (screen);
  gtk_window_set_title (GTK_WINDOW (window),
                        title && title[0] ? title : _("Terminal"));
}

static void
screen_font_any_changed_cb (TerminalScreen *screen,
                            GParamSpec *psepc,
                            TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  terminal_window_update_size (window);
}

static void
screen_hyperlink_hover_uri_changed (TerminalScreen *screen,
                                    const char *uri,
                                    const GdkRectangle *bbox G_GNUC_UNUSED,
                                    TerminalWindow *window G_GNUC_UNUSED)
{
  gs_free char *label = nullptr;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen)))
    return;

  label = terminal_util_hyperlink_uri_label (uri);

  gtk_widget_set_tooltip_text (GTK_WIDGET (screen), label);
}

/* MDI container callbacks */

static void
screen_close_request_cb (TerminalMdiContainer *container,
                         TerminalScreen *screen,
                         TerminalWindow *window)
{
  if (confirm_close_window_or_tab (window, screen))
    return;

  terminal_window_remove_screen (window, screen);
}

int
terminal_window_get_active_screen_num (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  return terminal_mdi_container_get_active_screen_num (priv->mdi_container);
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen,
                            int            position)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *old_window;

  old_window = gtk_widget_get_toplevel (GTK_WIDGET (screen));
  if (gtk_widget_is_toplevel (old_window) &&
      TERMINAL_IS_WINDOW (old_window) &&
      TERMINAL_WINDOW (old_window)== window)
    return;

  if (TERMINAL_IS_WINDOW (old_window))
    terminal_window_remove_screen (TERMINAL_WINDOW (old_window), screen);

  if (position == -1) {
    GSettings *global_settings = terminal_app_get_global_settings (terminal_app_get ());
    TerminalNewTabPosition position_pref = TerminalNewTabPosition
      (g_settings_get_enum (global_settings,
			    TERMINAL_SETTING_NEW_TAB_POSITION_KEY));
    switch (position_pref) {
    case TERMINAL_NEW_TAB_POSITION_NEXT:
      position = terminal_window_get_active_screen_num (window) + 1;
      break;

    default:
    case TERMINAL_NEW_TAB_POSITION_LAST:
      position = -1;
      break;
    }
  }

  terminal_mdi_container_add_screen (priv->mdi_container, screen, position);
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_remove_screen (priv->mdi_container, screen);
}

void
terminal_window_move_screen (TerminalWindow *source_window,
                             TerminalWindow *dest_window,
                             TerminalScreen *screen,
                             int dest_position)
{
  TerminalScreenContainer *screen_container;

  g_return_if_fail (TERMINAL_IS_WINDOW (source_window));
  g_return_if_fail (TERMINAL_IS_WINDOW (dest_window));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_get_toplevel (GTK_WIDGET (screen)) == GTK_WIDGET (source_window));
  g_return_if_fail (dest_position >= -1);

  screen_container = terminal_screen_container_get_from_screen (screen);
  g_assert (TERMINAL_IS_SCREEN_CONTAINER (screen_container));

  /* We have to ref the screen container as well as the screen,
   * because otherwise removing the screen container from the source
   * window's notebook will cause the container and its containing
   * screen to be gtk_widget_destroy()ed!
   */
  g_object_ref_sink (screen_container);
  g_object_ref_sink (screen);
  terminal_window_remove_screen (source_window, screen);

  /* Now we can safely remove the screen from the container and let the container die */
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (screen))), GTK_WIDGET (screen));
  g_object_unref (screen_container);

  terminal_window_add_screen (dest_window, screen, dest_position);
  terminal_mdi_container_set_active_screen (dest_window->priv->mdi_container, screen);
  g_object_unref (screen);
}

GList*
terminal_window_list_screen_containers (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return terminal_mdi_container_list_screen_containers (priv->mdi_container);
}

void
terminal_window_set_menubar_visible (TerminalWindow *window,
                                     gboolean        setting)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->menubar == nullptr)
    return;

  /* it's been set now, so don't override when adding a screen.
   * this side effect must happen before we short-circuit below.
   */
  priv->use_default_menubar_visibility = FALSE;

  g_simple_action_set_state (lookup_action (window, "menubar-visible"),
                             g_variant_new_boolean (setting));

  g_object_set (priv->menubar, "visible", setting, nullptr);

  /* FIXMEchpe: use gtk_widget_get_realized instead? */
  if (priv->active_screen)
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] setting size after toggling menubar visibility\n",
                             window);

      terminal_window_update_size (window);
    }
}

GtkWidget *
terminal_window_get_mdi_container (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), nullptr);

  return GTK_WIDGET (priv->mdi_container);
}

void
terminal_window_update_size (TerminalWindow *window)
{
  auto const priv = window->priv;
  int grid_width, grid_height;
  int pixel_width, pixel_height;

  if (priv->realized &&
      window_state_is_snapped(priv->window_state))
    {
      /* Don't adjust the size of maximized or tiled (snapped, half-maximized)
       * windows: if we do, there will be ugly gaps of up to 1 character cell
       * around otherwise tiled windows. */
      return;
    }

  if (!priv->active_screen)
    return;

  /* be sure our geometry is up-to-date */
  terminal_window_update_geometry (window);

  terminal_screen_get_size (priv->active_screen, &grid_width, &grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] size is %dx%d cells of %dx%d px\n",
                         window, grid_width, grid_height,
                         priv->old_char_width, priv->old_char_height);

  /* the "old" struct members were updated by update_geometry */
  pixel_width = priv->old_chrome_width + grid_width * priv->old_char_width;
  pixel_height = priv->old_chrome_height + grid_height * priv->old_char_height;
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] %dx%d + %dx%d = %dx%d\n",
                         window, grid_width * priv->old_char_width,
                         grid_height * priv->old_char_height,
                         priv->old_chrome_width, priv->old_chrome_height,
                         pixel_width, pixel_height);

  gtk_window_resize (GTK_WINDOW (window), pixel_width, pixel_height);
}

void
terminal_window_switch_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_set_active_screen (priv->mdi_container, screen);
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return terminal_mdi_container_get_active_screen (priv->mdi_container);
}

static void
notebook_show_context_menu (TerminalWindow *window,
                            GdkEvent *event,
                            guint button,
                            guint32 timestamp)
{
  /* Load the UI */
  gs_unref_object GMenu *menu;
  terminal_util_load_objects_resource ("/org/gnome/terminal/ui/notebook-menu.ui",
                                       "notebook-popup", &menu,
                                       nullptr);

  GtkWidget *popup_menu = context_menu_new (G_MENU_MODEL (menu), GTK_WIDGET (window));

  gtk_widget_set_halign (popup_menu, GTK_ALIGN_START);
  gtk_menu_popup (GTK_MENU (popup_menu), nullptr, nullptr,
                  nullptr, nullptr,
                  button, timestamp);

  if (button == 0)
    gtk_menu_shell_select_first (GTK_MENU_SHELL (popup_menu), FALSE);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
                          GdkEventButton *event,
                          TerminalWindow *window)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  int tab_clicked;

  if (event->type != GDK_BUTTON_PRESS ||
      event->button != GDK_BUTTON_SECONDARY ||
      (event->state & gtk_accelerator_get_default_mod_mask ()) != 0)
    return FALSE;

  tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);
  if (tab_clicked < 0)
    return FALSE;

  /* switch to the page the mouse is over */
  gtk_notebook_set_current_page (notebook, tab_clicked);

  notebook_show_context_menu (window, (GdkEvent*)event, event->button, event->time);
  return TRUE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *focus_widget;

  focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
  /* Only respond if the notebook is the actual focus */
  if (focus_widget != GTK_WIDGET (priv->mdi_container))
    return FALSE;

  notebook_show_context_menu (window, nullptr, 0, gtk_get_current_event_time ());
  return TRUE;
}

static void
mdi_screen_switched_cb (TerminalMdiContainer *container,
                        TerminalScreen *old_active_screen,
                        TerminalScreen *screen,
                        TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int old_grid_width, old_grid_height;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen-switched old %p new %p\n",
                         window, old_active_screen, screen);

  if (priv->disposed)
    return;

  if (screen == nullptr || old_active_screen == screen)
    return;

  if (priv->search_popover != nullptr)
    gtk_widget_hide (GTK_WIDGET (priv->search_popover));

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: setting active tab to screen %p (old active screen %p)\n",
                         window, screen, priv->active_screen);

  if (old_active_screen != nullptr && screen != nullptr) {
    terminal_screen_get_size (old_active_screen, &old_grid_width, &old_grid_height);

    /* This is so that we maintain the same grid */
    vte_terminal_set_size (VTE_TERMINAL (screen), old_grid_width, old_grid_height);
  }

  priv->active_screen = screen;

  /* Override menubar setting if it wasn't restored from session */
  if (priv->use_default_menubar_visibility)
    {
      gboolean setting =
        g_settings_get_boolean (terminal_app_get_global_settings (terminal_app_get ()),
                                TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR_KEY);

      terminal_window_set_menubar_visible (window, setting);
    }

  sync_screen_title (screen, nullptr, window);

  /* set size of window to current grid size */
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] setting size after flipping notebook pages\n",
                         window);
  terminal_window_update_size (window);

  terminal_window_update_tabs_actions_sensitivity (window);
  terminal_window_update_terminal_menu (window);
  terminal_window_update_set_profile_menu_active_profile (window);
  terminal_window_update_copy_sensitivity (screen, window);
  terminal_window_update_zoom_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);
  terminal_window_update_paste_sensitivity (window);
}

static void
mdi_screen_added_cb (TerminalMdiContainer *container,
                     TerminalScreen *screen,
                     TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int pages;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p inserted\n",
                         window, screen);

  g_signal_connect (G_OBJECT (screen),
                    "profile-set",
                    G_CALLBACK (profile_set_cb),
                    window);

  /* FIXME: only connect on the active screen, not all screens! */
  g_signal_connect (screen, "notify::title",
                    G_CALLBACK (sync_screen_title), window);
  g_signal_connect (screen, "notify::font-desc",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "notify::font-scale",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "notify::cell-height-scale",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "notify::cell-width-scale",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "selection-changed",
                    G_CALLBACK (terminal_window_update_copy_sensitivity), window);
  g_signal_connect (screen, "hyperlink-hover-uri-changed",
                    G_CALLBACK (screen_hyperlink_hover_uri_changed), window);

  g_signal_connect (screen, "show-popup-menu",
                    G_CALLBACK (screen_show_popup_menu_cb), window);
  g_signal_connect (screen, "match-clicked",
                    G_CALLBACK (screen_match_clicked_cb), window);
  g_signal_connect (screen, "resize-window",
                    G_CALLBACK (screen_resize_window_cb), window);

  g_signal_connect (screen, "close-screen",
                    G_CALLBACK (screen_close_cb), window);

  terminal_window_update_tabs_actions_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);
  terminal_window_update_paste_sensitivity (window);

#if 0
  /* FIXMEchpe: wtf is this doing? */

  /* If we have an active screen, match its size and zoom */
  if (priv->active_screen)
    {
      int current_width, current_height;
      double scale;

      terminal_screen_get_size (priv->active_screen, &current_width, &current_height);
      vte_terminal_set_size (VTE_TERMINAL (screen), current_width, current_height);

      scale = terminal_screen_get_font_scale (priv->active_screen);
      terminal_screen_set_font_scale (screen, scale);
    }
#endif

  if (priv->present_on_insert)
    {
      gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());
      priv->present_on_insert = FALSE;
    }

  pages = terminal_mdi_container_get_n_screens (container);
  if (pages == 2)
    {
      terminal_window_update_size (window);
    }
}

static void
mdi_screen_removed_cb (TerminalMdiContainer *container,
                       TerminalScreen *screen,
                       TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int pages;

  if (priv->disposed)
    return;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p removed\n",
                         window, screen);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)profile_set_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)sync_screen_title,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)screen_font_any_changed_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)terminal_window_update_copy_sensitivity,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)screen_hyperlink_hover_uri_changed,
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_show_popup_menu_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_match_clicked_cb,
                                        window);
  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_resize_window_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_close_cb,
                                        window);

  /* We already got a switch-page signal whose handler sets the active tab to the
   * new active tab, unless this screen was the only one in the notebook, so
   * priv->active_tab is valid here.
   */

  pages = terminal_mdi_container_get_n_screens (container);
  if (pages == 0)
    {
      priv->active_screen = nullptr;

      /* That was the last tab in the window; close it. */
      gtk_widget_destroy (GTK_WIDGET (window));
      return;
    }

  terminal_window_update_tabs_actions_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);

  if (pages == 1)
    {
      TerminalScreen *active_screen = terminal_mdi_container_get_active_screen (container);
      gtk_widget_grab_focus (GTK_WIDGET(active_screen));  /* bug 742422 */

      terminal_window_update_size (window);
    }
}

static void
mdi_screens_reordered_cb (TerminalMdiContainer *container,
                          TerminalWindow  *window)
{
  terminal_window_update_tabs_actions_sensitivity (window);
}

gboolean
terminal_window_parse_geometry (TerminalWindow *window,
				const char     *geometry)
{
  TerminalWindowPrivate *priv = window->priv;

  /* gtk_window_parse_geometry() needs to have the right base size
   * and width/height increment to compute the window size from
   * the geometry.
   */
  terminal_window_update_geometry (window);

  if (!gtk_window_parse_geometry (GTK_WINDOW (window), geometry))
    return FALSE;

  /* We won't actually get allocated at the size parsed out of the
   * geometry until the window is shown. If terminal_window_update_size()
   * is called between now and then, that could result in us getting
   * snapped back to the old grid size. So we need to immediately
   * update the size of the active terminal to grid size from the
   * geometry.
   */
  if (priv->active_screen)
    {
      int grid_width, grid_height;

      /* After parse_geometry(), the default size is in units of the
       * width/height increment, not a pixel size */
      gtk_window_get_default_size (GTK_WINDOW (window), &grid_width, &grid_height);

      vte_terminal_set_size (VTE_TERMINAL (priv->active_screen),
			     grid_width, grid_height);
    }

  return TRUE;
}

void
terminal_window_update_geometry (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;
  GdkGeometry *hints = &priv->hints;
  GtkBorder padding;
  GtkRequisition vbox_request, widget_request;
  int grid_width, grid_height;
  int char_width, char_height;
  int chrome_width, chrome_height;
  int csd_width = 0, csd_height = 0;

  if (gtk_widget_in_destruction (GTK_WIDGET (window)))
    return;

  if (priv->active_screen == nullptr)
    return;

  widget = GTK_WIDGET (priv->active_screen);

  /* We set geometry hints from the active term; best thing
   * I can think of to do. Other option would be to try to
   * get some kind of union of all hints from all terms in the
   * window, but that doesn't make too much sense.
   */
  terminal_screen_get_cell_size (priv->active_screen, &char_width, &char_height);

  terminal_screen_get_size (priv->active_screen, &grid_width, &grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "%dx%d cells of %dx%d px = %dx%d px\n",
                         grid_width, grid_height, char_width, char_height,
                         char_width * grid_width, char_height * grid_height);

  gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                gtk_widget_get_state_flags(widget),
                                &padding);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "padding = %dx%d px\n",
                         padding.left + padding.right,
                         padding.top + padding.bottom);

  gtk_widget_get_preferred_size (priv->main_vbox, nullptr, &vbox_request);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "content area requests %dx%d px\n",
                         vbox_request.width, vbox_request.height);


  chrome_width = vbox_request.width - (char_width * grid_width);
  chrome_height = vbox_request.height - (char_height * grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "chrome: %dx%d px\n",
                         chrome_width, chrome_height);

  if (priv->realized)
    {
      /* Only when having been realize the CSD can be calculated. Do this by
       * using the actual allocation rather then the preferred size as the
       * the preferred size takes the natural size of e.g. the title bar into
       * account which can be far wider then the contents size when using a
       * very long title */
      GtkAllocation toplevel_allocation, vbox_allocation;

      gtk_widget_get_allocation (GTK_WIDGET (priv->main_vbox), &vbox_allocation);
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "terminal widget allocation %dx%d px\n",
                         vbox_allocation.width, vbox_allocation.height);

      gtk_widget_get_allocation (GTK_WIDGET (window), &toplevel_allocation);
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "window allocation %dx%d px\n",
                         toplevel_allocation.width, toplevel_allocation.height);

      csd_width =  toplevel_allocation.width - vbox_allocation.width;
      csd_height = toplevel_allocation.height - vbox_allocation.height;
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "CSDs: %dx%d px\n",
                             csd_width, csd_height);
    }

  gtk_widget_get_preferred_size (widget, nullptr, &widget_request);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "terminal widget requests %dx%d px\n",
                         widget_request.width, widget_request.height);

  if (!priv->realized)
    {
      /* Don't actually set the geometry hints until we have been realized,
       * because we don't know how large the client-side decorations are going
       * to be. We also avoid setting priv->old_csd_width or
       * priv->old_csd_height, so that next time through this function we'll
       * definitely recalculate the hints.
       *
       * Similarly, the size request doesn't seem to include the padding
       * until we've been redrawn at least once. Don't resize the window
       * until we've done that. */
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "not realized yet\n");
    }
  else if (window_state_is_snapped(priv->window_state))
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "Not applying geometry in snapped state\n");
    }
  else if (char_width != priv->old_char_width ||
      char_height != priv->old_char_height ||
      padding.left + padding.right != priv->old_padding_width ||
      padding.top + padding.bottom != priv->old_padding_height ||
      chrome_width != priv->old_chrome_width ||
      chrome_height != priv->old_chrome_height ||
      csd_width != priv->old_csd_width ||
      csd_height != priv->old_csd_height ||
      widget != (GtkWidget*) priv->old_geometry_widget)
    {
      hints->base_width = chrome_width + csd_width;
      hints->base_height = chrome_height + csd_height;

      hints->width_inc = char_width;
      hints->height_inc = char_height;

      /* min size is min size of the whole window, remember. */
      hints->min_width = hints->base_width + hints->width_inc * MIN_WIDTH_CHARS;
      hints->min_height = hints->base_height + hints->height_inc * MIN_HEIGHT_CHARS;

      gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                     nullptr,
                                     hints,
                                     GdkWindowHints(GDK_HINT_RESIZE_INC |
						    GDK_HINT_MIN_SIZE |
						    GDK_HINT_BASE_SIZE));

      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: base %dx%d min %dx%d inc %d %d\n",
                             window,
                             hints->base_width,
                             hints->base_height,
                             hints->min_width,
                             hints->min_height,
                             hints->width_inc,
                             hints->height_inc);

      priv->old_csd_width = csd_width;
      priv->old_csd_height = csd_height;
      priv->old_geometry_widget = widget;
    }
  else
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: increment unchanged, not setting\n",
                             window);
    }

  /* We need these for the size calculation in terminal_window_update_size()
   * (at least under GTK >= 3.19), so we set them unconditionally. */
  priv->old_char_width = char_width;
  priv->old_char_height = char_height;
  priv->old_chrome_width = chrome_width;
  priv->old_chrome_height = chrome_height;
  priv->old_padding_width = padding.left + padding.right;
  priv->old_padding_height = padding.top + padding.bottom;
}

static void
confirm_close_response_cb (GtkWidget *dialog,
                           int response,
                           TerminalWindow *window)
{
  TerminalScreen *screen;

  screen = (TerminalScreen*)g_object_get_data (G_OBJECT (dialog), "close-screen");

  gtk_widget_destroy (dialog);

  if (response != GTK_RESPONSE_ACCEPT)
    return;
    
  if (screen)
    terminal_window_remove_screen (window, screen);
  else
    gtk_widget_destroy (GTK_WIDGET (window));
}

/* Returns: TRUE if closing needs to wait until user confirmation;
 * FALSE if the terminal or window can close immediately.
 */
static gboolean
confirm_close_window_or_tab (TerminalWindow *window,
                             TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *dialog;
  gboolean do_confirm;
  int n_tabs;

  if (priv->confirm_close_dialog)
    {
      /* WTF, already have one? It's modal, so how did that happen? */
      gtk_dialog_response (GTK_DIALOG (priv->confirm_close_dialog),
                           GTK_RESPONSE_DELETE_EVENT);
    }

  do_confirm = g_settings_get_boolean (terminal_app_get_global_settings (terminal_app_get ()),
                                       TERMINAL_SETTING_CONFIRM_CLOSE_KEY);
  if (!do_confirm)
    return FALSE;

  if (screen)
    {
      do_confirm = terminal_screen_has_foreground_process (screen, nullptr, nullptr);
      n_tabs = 1;
    }
  else
    {
      GList *tabs, *t;

      do_confirm = FALSE;

      tabs = terminal_window_list_screen_containers (window);
      n_tabs = g_list_length (tabs);

      for (t = tabs; t != nullptr; t = t->next)
        {
          TerminalScreen *terminal_screen;

          terminal_screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (t->data));
          if (terminal_screen_has_foreground_process (terminal_screen, nullptr, nullptr))
            {
              do_confirm = TRUE;
              break;
            }
        }
      g_list_free (tabs);
    }

  if (!do_confirm)
    return FALSE;

  dialog = priv->confirm_close_dialog =
    gtk_message_dialog_new (GTK_WINDOW (window),
                            GtkDialogFlags(GTK_DIALOG_MODAL |
					   GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_WARNING,
                            GTK_BUTTONS_CANCEL,
                            "%s", n_tabs > 1 ? _("Close this window?") : _("Close this terminal?"));

  if (n_tabs > 1)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", _("There are still processes running in some terminals in this window. "
                                                      "Closing the window will kill all of them."));
  else
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", _("There is still a process running in this terminal. "
                                                      "Closing the terminal will kill it."));

  gtk_window_set_title (GTK_WINDOW (dialog), ""); 

  GtkWidget *remove_button = gtk_dialog_add_button (GTK_DIALOG (dialog), n_tabs > 1 ? _("C_lose Window") : _("C_lose Terminal"), GTK_RESPONSE_ACCEPT);
  gtk_style_context_add_class (gtk_widget_get_style_context (remove_button), "destructive-action");
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  g_object_set_data (G_OBJECT (dialog), "close-screen", screen);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed), &priv->confirm_close_dialog);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (confirm_close_response_cb), window);

  gtk_window_present (GTK_WINDOW (dialog));

  return TRUE;
}

void
terminal_window_request_close (TerminalWindow *window)
{
  g_return_if_fail (TERMINAL_IS_WINDOW (window));

  if (confirm_close_window_or_tab (window, nullptr))
    return;

  gtk_widget_destroy (GTK_WIDGET (window));
}

const char *
terminal_window_get_uuid (TerminalWindow *window)
{
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), nullptr);

  return window->priv->uuid;
}
