/*
 * Copyright © 2013, 2014 Red Hat, Inc.
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

#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-libgsystem.hh"
#include "terminal-screen-container.hh"
#include "terminal-search-provider.hh"
#include "terminal-search-provider-gdbus-generated.h"
#include "terminal-window.hh"

struct _TerminalSearchProvider
{
  GObject parent;

  TerminalSearchProvider2 *skeleton;
};

struct _TerminalSearchProviderClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (TerminalSearchProvider, terminal_search_provider, G_TYPE_OBJECT)

static char *
normalize_casefold_and_unaccent (const char *str)
{
  gs_free char *casefolded = nullptr, *normalized = nullptr;
  char *retval = nullptr;

  if (str == nullptr)
    goto out;

  normalized = g_utf8_normalize (str, -1, G_NORMALIZE_ALL_COMPOSE);
  casefolded = g_utf8_casefold (normalized, -1);
  retval = g_str_to_ascii (casefolded, nullptr);

 out:
  return retval;
}

static char **
normalize_casefold_and_unaccent_terms (const char* const *terms)
{
  char **casefolded_terms;
  guint i, n;

  n = g_strv_length ((char **) terms);
  casefolded_terms = g_new (char *, n + 1);

  for (i = 0; i < n; i++)
    casefolded_terms[i] = normalize_casefold_and_unaccent (terms[i]);
  casefolded_terms[n] = nullptr;

  return casefolded_terms;
}

static gboolean
match_terms (const char        *str,
             const char* const *terms)
{
  gs_free char *casefolded_str = nullptr;
  gboolean matches = TRUE;
  guint i;

  if (str == nullptr)
    {
      matches = FALSE;
      goto out;
    }

  casefolded_str = normalize_casefold_and_unaccent (str);
  for (i = 0; terms[i] != nullptr; i++)
    {
      if (strstr (casefolded_str, terms[i]) == nullptr)
        {
          matches = FALSE;
          break;
        }
    }

 out:
  return matches;
}

static gboolean
handle_get_initial_result_set_cb (TerminalSearchProvider2  *skeleton,
                                  GDBusMethodInvocation    *invocation,
                                  const char *const        *terms,
                                  gpointer                  user_data)
{
  GList *l, *screens = nullptr, *windows;
  gs_unref_ptrarray GPtrArray *results;
  TerminalApp *app;
  gs_strfreev char **casefolded_terms = nullptr;

  _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "GetInitialResultSet started\n");

  app = terminal_app_get ();
  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  for (l = windows; l != nullptr; l = l->next)
    {
      TerminalWindow *window = (TerminalWindow*)(l->data);
      GList *c, *containers;

      if (!TERMINAL_IS_WINDOW (l->data))
        continue;

      containers = terminal_window_list_screen_containers (window);
      for (c = containers; c != nullptr; c = c->next)
        {
          TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (c->data);
          TerminalScreen *screen;

          screen = terminal_screen_container_get_screen (container);
          screens = g_list_prepend (screens, screen);
        }
    }

  casefolded_terms = normalize_casefold_and_unaccent_terms (terms);
  results = g_ptr_array_new_with_free_func (g_free);

  for (l = screens; l != nullptr; l = l->next)
    {
      TerminalScreen *screen = TERMINAL_SCREEN (l->data);
      gs_free char *cmdline = nullptr, *process = nullptr;
      const char *cwd, *title;

      cwd = vte_terminal_get_current_directory_uri (VTE_TERMINAL (screen));
      title = terminal_screen_get_title (screen);
      terminal_screen_has_foreground_process (screen, &process, &cmdline);
      if (match_terms (cwd, (const char *const *) casefolded_terms) ||
          match_terms (title, (const char *const *) casefolded_terms) ||
          match_terms (process, (const char *const *) casefolded_terms) ||
          match_terms (cmdline, (const char *const *) casefolded_terms))
        {
          const char *uuid;

          uuid = terminal_screen_get_uuid (screen);
          g_ptr_array_add (results, g_strdup (uuid));

          _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "Search hit: %s\n", uuid);
        }
    }

  g_ptr_array_add (results, nullptr);
  terminal_search_provider2_complete_get_initial_result_set (skeleton,
                                                             invocation,
                                                             (const char *const *) results->pdata);

  _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "GetInitialResultSet completed\n");
  return TRUE;
}

static gboolean
handle_get_subsearch_result_set_cb (TerminalSearchProvider2  *skeleton,
                                    GDBusMethodInvocation    *invocation,
                                    const char *const        *previous_results,
                                    const char *const        *terms,
                                    gpointer                  user_data)
{
  gs_unref_ptrarray GPtrArray *results;
  TerminalApp *app;
  gs_strfreev char **casefolded_terms = nullptr;
  guint i;

  _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "GetSubsearchResultSet started\n");

  app = terminal_app_get ();
  casefolded_terms = normalize_casefold_and_unaccent_terms (terms);
  results = g_ptr_array_new_with_free_func (g_free);

  for (i = 0; previous_results[i] != nullptr; i++)
    {
      TerminalScreen *screen;
      gs_free char *cmdline = nullptr, *process = nullptr;
      const char *cwd, *title;

      screen = terminal_app_get_screen_by_uuid (app, previous_results[i]);
      if (screen == nullptr)
        {
          _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "Not a screen: %s\n", previous_results[i]);
          continue;
        }

      cwd = vte_terminal_get_current_directory_uri (VTE_TERMINAL (screen));
      title = terminal_screen_get_title (screen);
      terminal_screen_has_foreground_process (screen, &process, &cmdline);
      if (match_terms (cwd, (const char *const *) casefolded_terms) ||
          match_terms (title, (const char *const *) casefolded_terms) ||
          match_terms (process, (const char *const *) casefolded_terms) ||
          match_terms (cmdline, (const char *const *) casefolded_terms))
        {
          g_ptr_array_add (results, g_strdup (previous_results[i]));
          _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "Search hit: %s\n", previous_results[i]);
        }
    }

  g_ptr_array_add (results, nullptr);
  terminal_search_provider2_complete_get_subsearch_result_set (skeleton,
                                                               invocation,
                                                               (const char *const *) results->pdata);

  _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "GetSubsearchResultSet completed\n");
  return TRUE;
}

static gboolean
handle_get_result_metas_cb (TerminalSearchProvider2  *skeleton,
                            GDBusMethodInvocation    *invocation,
                            const char *const        *results,
                            gpointer                  user_data)
{
  GVariantBuilder builder;
  TerminalApp *app;
  guint i;

  _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "GetResultMetas started\n");

  app = terminal_app_get ();
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (i = 0; results[i] != nullptr; i++)
    {
      TerminalScreen *screen;
      const char *title;
      gs_free char *escaped_text = nullptr;
      gs_free char *text = nullptr;

      screen = terminal_app_get_screen_by_uuid (app, results[i]);
      if (screen == nullptr)
        {
          _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "Not a screen: %s\n", results[i]);
          continue;
        }

      title = terminal_screen_get_title (screen);
      if (terminal_screen_has_foreground_process (screen, nullptr, nullptr)) {
        VteTerminal *terminal = VTE_TERMINAL (screen);
        long cursor_row;

        vte_terminal_get_cursor_position (terminal, nullptr, &cursor_row);
        text = vte_terminal_get_text_range (terminal,
                                            MAX(0, cursor_row - 1),
                                            0,
                                            cursor_row + 1,
                                            vte_terminal_get_column_count (terminal) - 1,
                                            nullptr, nullptr, nullptr);
      }

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}", "id", g_variant_new_string (results[i]));
      if (title && title[0])
        g_variant_builder_add (&builder, "{sv}", "name", g_variant_new_string (title));
      if (text != nullptr)
        {
          escaped_text = g_markup_escape_text (text, -1);
          g_variant_builder_add (&builder, "{sv}", "description", g_variant_new_string (escaped_text));
        }
      g_variant_builder_close (&builder);

      _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "Meta for %s: %s\n", results[i], title ? title : "(null)");
    }

  terminal_search_provider2_complete_get_result_metas (skeleton, invocation, g_variant_builder_end (&builder));

  _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "GetResultMetas completed\n");

  return TRUE;
}

static gboolean
handle_activate_result_cb (TerminalSearchProvider2  *skeleton,
                           GDBusMethodInvocation    *invocation,
                           const char               *identifier,
                           const char* const        *terms,
                           guint                     timestamp,
                           gpointer                  user_data)
{
  GtkWidget *toplevel;
  TerminalApp *app;
  TerminalScreen *screen;

  app = terminal_app_get ();
  screen = terminal_app_get_screen_by_uuid (app, identifier);
  if (screen == nullptr)
    goto out;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (screen));
  if (!gtk_widget_is_toplevel (toplevel))
    goto out;

  terminal_window_switch_screen (TERMINAL_WINDOW (toplevel), screen);
  gtk_window_present_with_time (GTK_WINDOW (toplevel), timestamp);
  _terminal_debug_print (TERMINAL_DEBUG_SEARCH, "ActivateResult: %s\n", identifier);

 out:
  terminal_search_provider2_complete_activate_result (skeleton, invocation);
  return TRUE;
}

static void
terminal_search_provider_init (TerminalSearchProvider *provider)
{
  provider->skeleton = terminal_search_provider2_skeleton_new ();

  g_signal_connect (provider->skeleton, "handle-get-initial-result-set",
                    G_CALLBACK (handle_get_initial_result_set_cb), provider);
  g_signal_connect (provider->skeleton, "handle-get-subsearch-result-set",
                    G_CALLBACK (handle_get_subsearch_result_set_cb), provider);
  g_signal_connect (provider->skeleton, "handle-get-result-metas",
                    G_CALLBACK (handle_get_result_metas_cb), provider);
  g_signal_connect (provider->skeleton, "handle-activate-result",
                    G_CALLBACK (handle_activate_result_cb), provider);
}

static void
terminal_search_provider_dispose (GObject *object)
{
  TerminalSearchProvider *provider = TERMINAL_SEARCH_PROVIDER (object);

  g_clear_object (&provider->skeleton);

  G_OBJECT_CLASS (terminal_search_provider_parent_class)->dispose (object);
}

static void
terminal_search_provider_class_init (TerminalSearchProviderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = terminal_search_provider_dispose;
}

TerminalSearchProvider *
terminal_search_provider_new (void)
{
  return reinterpret_cast<TerminalSearchProvider*>
    (g_object_new (TERMINAL_TYPE_SEARCH_PROVIDER, nullptr));
}

gboolean
terminal_search_provider_dbus_register (TerminalSearchProvider  *provider,
                                        GDBusConnection         *connection,
                                        const char              *object_path,
                                        GError                 **error)
{
  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (provider->skeleton),
                                           connection,
                                           object_path,
                                           error);
}

void
terminal_search_provider_dbus_unregister (TerminalSearchProvider  *provider,
                                          GDBusConnection         *connection,
                                          const char              *object_path)
{
  if (g_dbus_interface_skeleton_has_connection (G_DBUS_INTERFACE_SKELETON (provider->skeleton), connection))
    g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (provider->skeleton),
                                                        connection);
}
