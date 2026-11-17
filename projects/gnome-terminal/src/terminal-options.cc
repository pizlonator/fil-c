/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2017 Christian Persch
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

#include <config.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "terminal-options.hh"
#include "terminal-client-utils.hh"
#include "terminal-defines.hh"
#include "terminal-schemas.hh"
#include "terminal-screen.hh"
#include "terminal-app.hh"
#include "terminal-util.hh"
#include "terminal-version.hh"
#include "terminal-libgsystem.hh"
#include "terminal-settings-utils.hh"

static int verbosity = 1;

void
terminal_fprintf (FILE* fp,
                  int verbosity_level,
                  const char* format,
                  ...)
{
  if (verbosity < verbosity_level)
    return;

  va_list args;
  va_start(args, format);
  gs_free char *str = g_strdup_vprintf(format, args);
  va_end(args);

  gs_strfreev char **lines = g_strsplit_set(str, "\n\r", -1);
  for (gsize i = 0; lines[i]; ++i) {
    if (lines[i][0] != '\0')
      g_fprintf(fp, "# %s\n", lines[i]);
  }
}

static TerminalVerbosity
verbosity_from_log_level (GLogLevelFlags log_level)
{
  guint level = log_level & G_LOG_LEVEL_MASK;
  TerminalVerbosity res;
  level = level & ~(level - 1); /* extract the highest bit */
  switch (level) {
  case G_LOG_LEVEL_DEBUG:
    res = TERMINAL_VERBOSITY_DEBUG;
    break;
  case G_LOG_LEVEL_INFO:
    res = TERMINAL_VERBOSITY_DETAIL;
    break;
  default:
    /* better display than lose important messages */
    res = TERMINAL_VERBOSITY_NORMAL;
  }
  return res;
}

/* Need to install a special log writer so we never output
 * anything without the '# ' prepended, in case --print-environment
 * is used.
 *
 * FIXME: Until issue glib#2087 is fixed, apply a simple log level filter
 * to prevent spamming dconf (and other) debug messages to stderr,
 * see issue gnome-terminal#42.
 */
GLogWriterOutput
terminal_log_writer (GLogLevelFlags log_level,
                     const GLogField *fields,
                     gsize n_fields,
                     gpointer user_data)
{
#if GLIB_CHECK_VERSION(2, 68, 0)
  char const* domain = nullptr;
  for (auto i = gsize{0}; i < n_fields; i++) {
    if (g_str_equal(fields[i].key, "GLIB_DOMAIN")) {
      domain = (char const*)fields[i].value;
      break;
    }
  }
  if (g_log_writer_default_would_drop(log_level, domain))
    return G_LOG_WRITER_HANDLED;
#endif /* glib 2.68 */

  TerminalVerbosity level = verbosity_from_log_level(log_level);
  for (gsize i = 0; i < n_fields; i++) {
    if (g_str_equal (fields[i].key, "MESSAGE"))
      terminal_fprintf (stderr, level, "%s\n", (const char*)fields[i].value);
  }

  return G_LOG_WRITER_HANDLED;
}

static GOptionContext *get_goption_context (TerminalOptions *options);

static void
terminal_options_ensure_schema_source(TerminalOptions* options)
{
  if (options->schema_source)
    return;

  options->schema_source = terminal_g_settings_schema_source_get_default();
}

static TerminalSettingsList *
terminal_options_ensure_profiles_list (TerminalOptions *options)
{
  if (options->profiles_list == nullptr) {
    terminal_options_ensure_schema_source(options);
    options->profiles_list = terminal_profiles_list_new(nullptr /* default backend */,
                                                        options->schema_source);
  }

  return options->profiles_list;
}

static char *
terminal_util_key_file_get_string_unescape (GKeyFile *key_file,
                                            const char *group,
                                            const char *key,
                                            GError **error)
{
  char *escaped, *unescaped;

  escaped = g_key_file_get_string (key_file, group, key, error);
  if (!escaped)
    return nullptr;

  unescaped = g_strcompress (escaped);
  g_free (escaped);

  return unescaped;
}

static char **
terminal_util_key_file_get_argv (GKeyFile *key_file,
                                 const char *group,
                                 const char *key,
                                 int *argc,
                                 GError **error)
{
  char **argv;
  char *flat;
  gboolean retval;

  flat = terminal_util_key_file_get_string_unescape (key_file, group, key, error);
  if (!flat)
    return nullptr;

  retval = g_shell_parse_argv (flat, argc, &argv, error);
  g_free (flat);

  if (retval)
    return argv;

  return nullptr;
}

static InitialTab*
initial_tab_new (char *profile /* adopts */)
{
  InitialTab *it;

  it = g_slice_new (InitialTab);

  it->profile = profile;
  it->exec_argv = nullptr;
  it->title = nullptr;
  it->working_dir = nullptr;
  it->zoom = 1.0;
  it->zoom_set = FALSE;
  it->active = FALSE;
  it->fd_list = nullptr;
  it->fd_array = nullptr;
  it->wait = false;

  return it;
}

static void
initial_tab_free (InitialTab *it)
{
  g_free (it->profile);
  g_strfreev (it->exec_argv);
  g_free (it->title);
  g_free (it->working_dir);
  g_clear_object (&it->fd_list);
  if (it->fd_array)
    g_array_unref (it->fd_array);
  g_slice_free (InitialTab, it);
}

static InitialWindow*
initial_window_new (guint source_tag)
{
  InitialWindow *iw;

  iw = g_slice_new0 (InitialWindow);
  iw->source_tag = source_tag;

  return iw;
}

static void
initial_window_free (InitialWindow *iw)
{
  g_list_free_full (iw->tabs, (GDestroyNotify) initial_tab_free);
  g_free (iw->geometry);
  g_free (iw->role);
  g_slice_free (InitialWindow, iw);
}

static void
apply_window_defaults (TerminalOptions *options,
                       InitialWindow *iw)
{
  if (options->default_role)
    {
      iw->role = options->default_role;
      options->default_role = nullptr;
    }

  if (iw->geometry == nullptr)
    iw->geometry = g_strdup (options->default_geometry);

  if (options->default_window_menubar_forced)
    {
      iw->force_menubar_state = TRUE;
      iw->menubar_state = options->default_window_menubar_state;

      options->default_window_menubar_forced = FALSE;
    }

  iw->start_fullscreen |= options->default_fullscreen;
  iw->start_maximized |= options->default_maximize;
}

static void
apply_tab_defaults (TerminalOptions *options,
                    InitialTab *it)
{
  it->wait = options->default_wait;
}

static InitialWindow*
add_new_window (TerminalOptions *options,
                char *profile /* adopts */,
                gboolean implicit_if_first_window)
{
  InitialWindow *iw;
  InitialTab *it;

  iw = initial_window_new (0);
  iw->implicit_first_window = (options->initial_windows == nullptr) && implicit_if_first_window;
  apply_window_defaults (options, iw);

  it = initial_tab_new (profile);

  /* If this is an implicit first window, the new tab should be active */
  if (iw->implicit_first_window)
    it->active = TRUE;

  iw->tabs = g_list_prepend (nullptr, it);
  apply_tab_defaults (options, it);

  options->initial_windows = g_list_append (options->initial_windows, iw);
  return iw;
}

static InitialWindow*
ensure_top_window (TerminalOptions *options,
                   gboolean implicit_if_first_window)
{
  InitialWindow *iw;

  if (options->initial_windows == nullptr)
    iw = add_new_window (options, nullptr /* profile */, implicit_if_first_window);
  else
    iw = (InitialWindow*)g_list_last (options->initial_windows)->data;

  g_assert_nonnull (iw->tabs);

  return iw;
}

static InitialTab*
ensure_top_tab (TerminalOptions *options)
{
  InitialWindow *iw;
  InitialTab *it;

  iw = ensure_top_window (options, TRUE);

  g_assert_nonnull (iw->tabs);

  it = (InitialTab*)g_list_last (iw->tabs)->data;

  return it;
}

/* handle deprecated command line options */

static void
deprecated_option_warning (const gchar *option_name)
{
  terminal_printerr (_("Option “%s” is deprecated and might be removed in a later version of gnome-terminal."),
                     option_name);
  terminal_printerr ("\n");
}

static void
deprecated_command_option_warning (const char *option_name)
{
  deprecated_option_warning (option_name);

  /* %s is being replaced with "-- " (without quotes), which must be used literally, not translatable */
  terminal_printerr (_("Use “%s” to terminate the options and put the command line to execute after it."), "-- ");
  terminal_printerr ("\n");
}

static gboolean
unsupported_option_callback (const gchar *option_name,
                             const gchar *value,
                             gpointer     data,
                             GError     **error)
{
  terminal_printerr (_("Option “%s” is no longer supported in this version of gnome-terminal."),
              option_name);
  terminal_printerr ("\n");
  return TRUE; /* we do not want to bail out here but continue */
}

static gboolean
unsupported_option_fatal_callback (const gchar *option_name,
                                   const gchar *value,
                                   gpointer     data,
                                   GError     **error)
{
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
               _("Option “%s” is no longer supported in this version of gnome-terminal."),
               option_name);
  return FALSE;
}


static gboolean G_GNUC_NORETURN
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  terminal_print ("GNOME Terminal %s using VTE %u.%u.%u %s\n",
                  VERSION,
                  vte_get_major_version (),
                  vte_get_minor_version (),
                  vte_get_micro_version (),
                  vte_get_features ());
  exit (EXIT_SUCCESS);
}

static gboolean
option_verbosity_cb (const gchar *option_name,
                     const gchar *value,
                     gpointer     data,
                     GError     **error)
{
  if (g_str_equal (option_name, "--quiet") || g_str_equal (option_name, "-q"))
    verbosity = 0;
  else
    verbosity++;

  return TRUE;
}

static gboolean
option_app_id_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;

  if (!g_application_id_is_valid (value)) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "\"%s\" is not a valid application ID", value);
    return FALSE;
  }

  g_free (options->server_app_id);
  options->server_app_id = g_strdup (value);

  return TRUE;
}

static gboolean
option_command_callback (const gchar *option_name,
                         const gchar *value,
                         gpointer     data,
                         GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  GError *err = nullptr;
  char  **exec_argv;

  deprecated_command_option_warning (option_name);

  // Already have argv e.g. via '--' ?
  if (options->exec_argv) {
    g_set_error (error,
                 G_OPTION_ERROR,
                 G_OPTION_ERROR_BAD_VALUE,
                 _("Cannot specify more than one command to run"));
    return FALSE;
  }

  if (!g_shell_parse_argv (value, nullptr, &exec_argv, &err))
    {
      g_set_error(error,
                  G_OPTION_ERROR,
                  G_OPTION_ERROR_BAD_VALUE,
                  _("Argument to “%s” is not a valid command: %s"),
                   "--command/-e",
                  err->message);
      g_error_free (err);
      return FALSE;
    }

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_strfreev (it->exec_argv);
      it->exec_argv = exec_argv;
    }
  else
    {
      g_strfreev (options->exec_argv);
      options->exec_argv = exec_argv;
    }

  return TRUE;
}

static gboolean
option_profile_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  char *profile;

  profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                     value, error);
  if (profile == nullptr)
  {
      terminal_printerr ("Profile '%s' specified but not found. Attempting to fall back "
                         "to the default profile.\n", value);
      g_clear_error (error);
      profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                         nullptr, error);
  }

  if (profile == nullptr)
      return FALSE;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->profile);
      it->profile = profile;
    }
  else
    {
      g_free (options->default_profile);
      options->default_profile = profile;
    }

  return TRUE;
}

static gboolean
option_profile_id_cb (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  char *profile;

  profile = terminal_profiles_list_dup_uuid (terminal_options_ensure_profiles_list (options),
                                             value, error);
  if (profile == nullptr)
    return FALSE;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->profile);
      it->profile = profile;
    }
  else
    {
      g_free (options->default_profile);
      options->default_profile = profile;
    }

  return TRUE;
}


static gboolean
option_window_callback (const gchar *option_name,
                        const gchar *value,
                        gpointer     data,
                        GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  char *profile;

  if (value != nullptr) {
    profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                       value, error);

    if (value && profile == nullptr) {
      terminal_printerr ("Profile '%s' specified but not found. Attempting to fall back "
                         "to the default profile.\n", value);
      g_clear_error (error);
      profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                         nullptr, error);
    }

    if (profile == nullptr)
      return FALSE;
  } else
    profile = nullptr;

  add_new_window (options, profile /* adopts */, FALSE);

  return TRUE;
}

static gboolean
option_tab_callback (const gchar *option_name,
                     const gchar *value,
                     gpointer     data,
                     GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  char *profile;

  if (value != nullptr) {
    profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                       value, error);
    if (profile == nullptr)
      return FALSE;
  } else
    profile = nullptr;

  if (options->initial_windows)
    {
      InitialWindow *iw;

      iw = (InitialWindow*)g_list_last (options->initial_windows)->data;
      iw->tabs = g_list_append (iw->tabs, initial_tab_new (profile /* adopts */));
    }
  else
    add_new_window (options, profile /* adopts */, TRUE);

  return TRUE;
}

static gboolean
option_role_callback (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = (InitialWindow*)g_list_last (options->initial_windows)->data;
      iw->role = g_strdup (value);
    }
  else if (!options->default_role)
    options->default_role = g_strdup (value);
  else
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "%s", _("Two roles given for one window"));
      return FALSE;
    }

  return TRUE;
}

static gboolean
option_show_menubar_callback (const gchar *option_name,
                              const gchar *value,
                              gpointer     data,
                              GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = (InitialWindow*)g_list_last (options->initial_windows)->data;
      if (iw->force_menubar_state && iw->menubar_state == TRUE)
        {
          terminal_printerr_detail (_("“%s” option given twice for the same window\n"),
                                    "--show-menubar");

          return TRUE;
        }

      iw->force_menubar_state = TRUE;
      iw->menubar_state = TRUE;
    }
  else
    {
      options->default_window_menubar_forced = TRUE;
      options->default_window_menubar_state = TRUE;
    }

  return TRUE;
}

static gboolean
option_hide_menubar_callback (const gchar *option_name,
                              const gchar *value,
                              gpointer     data,
                              GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = (InitialWindow*)g_list_last (options->initial_windows)->data;

      if (iw->force_menubar_state && iw->menubar_state == FALSE)
        {
          terminal_printerr_detail (_("“%s” option given twice for the same window\n"),
                                    "--hide-menubar");
          return TRUE;
        }

      iw->force_menubar_state = TRUE;
      iw->menubar_state = FALSE;
    }
  else
    {
      options->default_window_menubar_forced = TRUE;
      options->default_window_menubar_state = FALSE;
    }

  return TRUE;
}

static gboolean
option_maximize_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = (InitialWindow*)g_list_last (options->initial_windows)->data;
      iw->start_maximized = TRUE;
    }
  else
    options->default_maximize = TRUE;

  return TRUE;
}

static gboolean
option_fullscreen_callback (const gchar *option_name,
                            const gchar *value,
                            gpointer     data,
                            GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;

  if (options->initial_windows)
    {
      InitialWindow *iw;

      iw = (InitialWindow*)g_list_last (options->initial_windows)->data;
      iw->start_fullscreen = TRUE;
    }
  else
    options->default_fullscreen = TRUE;

  return TRUE;
}

static gboolean
option_geometry_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;

  if (options->initial_windows)
    {
      InitialWindow *iw;

      iw = (InitialWindow*)g_list_last (options->initial_windows)->data;
      iw->geometry = g_strdup (value);
    }
  else
    options->default_geometry = g_strdup (value);

  return TRUE;
}

static gboolean
option_load_config_cb (const gchar *option_name,
                       const gchar *value,
                       gpointer     data,
                       GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  GFile *file;
  char *config_file;
  GKeyFile *key_file;
  gboolean result;

  file = g_file_new_for_commandline_arg (value);
  config_file = g_file_get_path (file);
  g_object_unref (file);

  key_file = g_key_file_new ();
  result = g_key_file_load_from_file (key_file, config_file, GKeyFileFlags(0), error) &&
           terminal_options_merge_config (options, key_file,
                                          strcmp (option_name, "load-config") == 0 ? SOURCE_DEFAULT : SOURCE_SESSION,
                                          error);
  g_key_file_free (key_file);
  g_free (config_file);

  return result;
}

static gboolean
option_title_callback (const gchar *option_name,
                       const gchar *value,
                       gpointer     data,
                       GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->title);
      it->title = g_strdup (value);
    }
  else
    {
      g_free (options->default_title);
      options->default_title = g_strdup (value);
    }

  return TRUE;
}

static gboolean
option_working_directory_callback (const gchar *option_name,
                                   const gchar *value,
                                   gpointer     data,
                                   GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->working_dir);
      it->working_dir = g_strdup (value);
    }
  else
    {
      g_free (options->default_working_dir);
      options->default_working_dir = g_strdup (value);
    }

  return TRUE;
}

static gboolean
option_wait_cb (const gchar *option_name,
                const gchar *value,
                gpointer     data,
                GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);
      it->wait = TRUE;
    }
  else
    {
      options->default_wait = TRUE;
    }

  return TRUE;
}

static gboolean
option_pass_fd_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;

  errno = 0;
  char *end;
  gint64 v = g_ascii_strtoll (value, &end, 10);
  if (errno || end == value || v == -1 || v < G_MININT || v > G_MAXINT) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "Failed to parse \"%s\" as file descriptor number",
                 value);
    return FALSE;
  }

  int fd = v;
  if (fd == STDIN_FILENO ||
      fd == STDOUT_FILENO ||
      fd == STDERR_FILENO) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "FD passing of %s is not supported",
                 fd == STDIN_FILENO ? "stdin" : fd == STDOUT_FILENO ? "stdout" : "stderr");
    return FALSE;
  }

  InitialTab *it = ensure_top_tab (options);
  if (it->fd_list == nullptr)
    it->fd_list = g_unix_fd_list_new ();
  if (it->fd_array == nullptr)
    it->fd_array = g_array_sized_new (FALSE /* zero terminate */,
                                      TRUE /* clear */,
                                      sizeof (PassFdElement),
                                      8 /* that should be plenty */);


  for (guint i = 0; i < it->fd_array->len; i++) {
    PassFdElement *e = &g_array_index (it->fd_array, PassFdElement, i);
    if (e->fd == fd) {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   _("Cannot pass FD %d twice"), fd);
      return FALSE;
    }
  }

  int idx = g_unix_fd_list_append (it->fd_list, fd, error);
  if (idx == -1) {
    g_prefix_error (error, "%d: ", fd);
    return FALSE;
  }

  PassFdElement e = { idx, fd };
  g_array_append_val (it->fd_array, e);

#if 0
  if (fd == STDOUT_FILENO ||
      fd == STDERR_FILENO)
    verbosity = 0;
  if (fd == STDIN_FILENO)
    it->wait = TRUE;
#endif

  return TRUE;
}

static gboolean
option_active_callback (const gchar *option_name,
                        const gchar *value,
                        gpointer     data,
                        GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  InitialTab *it;

  it = ensure_top_tab (options);
  it->active = TRUE;

  return TRUE;
}

static gboolean
option_zoom_callback (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  double zoom;
  char *end;

  /* Try reading a locale-style double first, in case it was
    * typed by a person, then fall back to ascii_strtod (we
    * always save session in C locale format)
    */
  end = nullptr;
  errno = 0;
  zoom = g_strtod (value, &end);
  if (end == nullptr || *end != '\0')
    {
      g_set_error (error,
                   G_OPTION_ERROR,
                   G_OPTION_ERROR_BAD_VALUE,
                   _("“%s” is not a valid zoom factor"),
                   value);
      return FALSE;
    }

  if (zoom < (TERMINAL_SCALE_MINIMUM + 1e-6))
    {
      terminal_printerr (_("Zoom factor “%g” is too small, using %g\n"),
                         zoom,
                         TERMINAL_SCALE_MINIMUM);
      zoom = TERMINAL_SCALE_MINIMUM;
    }

  if (zoom > (TERMINAL_SCALE_MAXIMUM - 1e-6))
    {
      terminal_printerr (_("Zoom factor “%g” is too large, using %g\n"),
                         zoom,
                         TERMINAL_SCALE_MAXIMUM);
      zoom = TERMINAL_SCALE_MAXIMUM;
    }

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);
      it->zoom = zoom;
      it->zoom_set = TRUE;
    }
  else
    {
      options->zoom = zoom;
      options->zoom_set = TRUE;
    }

  return TRUE;
}

/* Evaluation of the arguments given to the command line options */
static gboolean
digest_options_callback (GOptionContext *context,
                         GOptionGroup *group,
                         gpointer      data,
                         GError      **error)
{
  TerminalOptions *options = (TerminalOptions*)data;
  InitialTab    *it;

  if (options->execute)
    {
      if (options->exec_argv == nullptr)
        {
          g_set_error (error,
                       G_OPTION_ERROR,
                       G_OPTION_ERROR_BAD_VALUE,
                       _("Option “%s” requires specifying the command to run"
                         " on the rest of the command line"),
                       "--execute/-x");
          return FALSE;
        }

      /* Apply -x/--execute command only to the first tab */
      it = ensure_top_tab (options);
      it->exec_argv = options->exec_argv;
      options->exec_argv = nullptr;
    }

  return TRUE;
}

static char*
getenv_utf8(char const* env)
{
  auto const value = g_getenv(env);
  if (!value ||
      !value[0] ||
      !g_utf8_validate(value, -1, nullptr))
    return nullptr;

  return g_strdup(value);
}

/**
 * terminal_options_parse:
 * @argcp: (inout) address of the argument count. Changed if any arguments were handled
 * @argvp: (inout) address of the argument vector. Any parameters understood by
 *   the terminal #GOptionContext are removed
 * @error: a #GError to fill in
 *
 * Parses the argument vector *@argvp.
 *
 * Returns: a new #TerminalOptions containing the windows and tabs to open,
 *   or %nullptr on error.
 */
TerminalOptions *
terminal_options_parse (int *argcp,
                        char ***argvp,
                        GError **error)
{
  TerminalOptions *options;
  GOptionContext *context;
  gboolean retval;
  int i;
  char **argv = *argvp;

  options = g_new0 (TerminalOptions, 1);

  options->print_environment = FALSE;
  options->default_window_menubar_forced = FALSE;
  options->default_window_menubar_state = TRUE;
  options->default_fullscreen = FALSE;
  options->default_maximize = FALSE;
  options->execute = FALSE;

  options->startup_id = getenv_utf8("DESKTOP_STARTUP_ID");
  options->activation_token = getenv_utf8("XDG_ACTIVATION_TOKEN");
  options->display_name = nullptr;
  options->initial_windows = nullptr;
  options->default_role = nullptr;
  options->default_geometry = nullptr;
  options->default_title = nullptr;
  options->zoom = 1.0;
  options->zoom_set = FALSE;

  options->default_working_dir = g_get_current_dir ();

  /* Collect info from gnome-terminal private env vars */
  const char *server_unique_name = g_getenv (TERMINAL_ENV_SERVICE_NAME);
  if (server_unique_name != nullptr) {
    if (g_dbus_is_unique_name (server_unique_name))
      options->server_unique_name = g_strdup (server_unique_name);
    else
      terminal_printerr ("Warning: %s set but \"%s\" is not a unique D-Bus name.\n",
                         TERMINAL_ENV_SERVICE_NAME,
                         server_unique_name);
  }

  const char *parent_screen_object_path = g_getenv (TERMINAL_ENV_SCREEN);
  if (parent_screen_object_path != nullptr) {
    if (g_variant_is_object_path (parent_screen_object_path))
      options->parent_screen_object_path = g_strdup (parent_screen_object_path);
    else
      terminal_printerr ("Warning: %s set but \"%s\" is not a valid D-Bus object path.\n",
                         TERMINAL_ENV_SCREEN,
                         parent_screen_object_path);
  }

  /* The old -x/--execute option is broken, so we need to pre-scan for it. */
  /* We now also support passing the command after the -- switch. */
  options->exec_argv = nullptr;
  for (i = 1 ; i < *argcp; ++i)
    {
      gboolean is_execute;
      gboolean is_dashdash;
      int j, last;

      is_execute = strcmp (argv[i], "-x") == 0 || strcmp (argv[i], "--execute") == 0;
      is_dashdash = strcmp (argv[i], "--") == 0;

      if (!is_execute && !is_dashdash)
        continue;

      if (is_execute)
        deprecated_command_option_warning (argv[i]);

      options->execute = is_execute;

      /* Skip the switch */
      last = i;
      ++i;
      if (i == *argcp)
        break; /* we'll complain about this later for -x/--execute; it's fine for -- */

      /* Collect the args, and remove them from argv */
      options->exec_argv = g_new0 (char*, *argcp - i + 1);
      for (j = 0; i < *argcp; ++i, ++j)
        options->exec_argv[j] = g_strdup (argv[i]);
      options->exec_argv[j] = nullptr;

      *argcp = last;
      break;
    }

  context = get_goption_context (options);
  retval = g_option_context_parse (context, argcp, argvp, error);
  g_option_context_free (context);

  if (!retval) {
    terminal_options_free (options);
    return nullptr;
  }

  // Error out if there are any arguments left after parsing
  if (*argcp > 1) {
    g_set_error (error,
                 G_OPTION_ERROR,
                 G_OPTION_ERROR_BAD_VALUE,
                 _("Too many arguments"));
    return nullptr;
  }

#ifdef GDK_WINDOWING_X11
  /* Do this here so that gdk_display is initialized */
  if (options->startup_id == nullptr) {
    options->startup_id = terminal_client_get_fallback_startup_id ();
  }
#endif /* X11 */

  GdkDisplay *display = gdk_display_get_default ();
  if (display != nullptr)
    options->display_name = g_strdup (gdk_display_get_name (display));

  /* Sanity check */
  guint wait = 0;
  for (GList *lw = options->initial_windows;  lw != nullptr; lw = lw->next) {
    InitialWindow *iw = (InitialWindow*)lw->data;
    for (GList *lt = iw->tabs; lt != nullptr; lt = lt->next) {
      InitialTab *it = (InitialTab*)lt->data;
      if (it->wait)
        wait++;
    }
  }

  if (wait > 1) {
    g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         _("Can only use --wait once"));
    return FALSE;
  }

  options->wait = wait != 0;
  return options;
}

/**
 * terminal_options_merge_config:
 * @options:
 * @key_file: a #GKeyFile containing to merge the options from
 * @source_tag: a source_tag to use in new #InitialWindow<!-- -->s
 * @error: a #GError to fill in
 *
 * Merges the saved options from @key_file into @options.
 *
 * Returns: %TRUE if @key_file was a valid key file containing a stored
 *   terminal configuration, or %FALSE on error
 */
gboolean
terminal_options_merge_config (TerminalOptions *options,
                               GKeyFile *key_file,
                               guint source_tag,
                               GError **error)
{
  int version, compat_version;
  char **groups;
  guint i;
  gboolean have_error = FALSE;
  GList *initial_windows = nullptr;

  if (!g_key_file_has_group (key_file, TERMINAL_CONFIG_GROUP))
    {
      g_set_error_literal (error, TERMINAL_OPTION_ERROR,
                           TERMINAL_OPTION_ERROR_INVALID_CONFIG_FILE,
                           _("Not a valid terminal config file."));
      return FALSE;
    }
  
  version = g_key_file_get_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_VERSION, nullptr);
  compat_version = g_key_file_get_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_COMPAT_VERSION, nullptr);

  if (version <= 0 ||
      compat_version <= 0 ||
      compat_version > TERMINAL_CONFIG_COMPAT_VERSION)
    {
      g_set_error_literal (error, TERMINAL_OPTION_ERROR,
                           TERMINAL_OPTION_ERROR_INCOMPATIBLE_CONFIG_FILE,
                           _("Incompatible terminal config file version."));
      return FALSE;
    }

  groups = g_key_file_get_string_list (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_WINDOWS, nullptr, error);
  if (!groups)
    return FALSE;

  for (i = 0; groups[i]; ++i)
    {
      const char *window_group = groups[i];
      char *active_terminal;
      char **tab_groups;
      InitialWindow *iw;
      guint j;

      tab_groups = g_key_file_get_string_list (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_TABS, nullptr, error);
      if (!tab_groups)
        continue; /* no tabs in this window, skip it */

      iw = initial_window_new (source_tag);
      initial_windows = g_list_append (initial_windows, iw);
      apply_window_defaults (options, iw);

      active_terminal = g_key_file_get_string (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_ACTIVE_TAB, nullptr);
      iw->role = g_key_file_get_string (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_ROLE, nullptr);
      iw->geometry = g_key_file_get_string (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_GEOMETRY, nullptr);
      iw->start_fullscreen = g_key_file_get_boolean (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_FULLSCREEN, nullptr);
      iw->start_maximized = g_key_file_get_boolean (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_MAXIMIZED, nullptr);
      if (g_key_file_has_key (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_MENUBAR_VISIBLE, nullptr))
        {
          iw->force_menubar_state = TRUE;
          iw->menubar_state = g_key_file_get_boolean (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_MENUBAR_VISIBLE, nullptr);
        }

      for (j = 0; tab_groups[j]; ++j)
        {
          const char *tab_group = tab_groups[j];
          InitialTab *it;
          char *profile;

          profile = g_key_file_get_string (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_PROFILE_ID, nullptr);
          it = initial_tab_new (profile /* adopts */);

          iw->tabs = g_list_append (iw->tabs, it);

          if (g_strcmp0 (active_terminal, tab_group) == 0)
            it->active = TRUE;

/*          it->width = g_key_file_get_integer (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_WIDTH, nullptr);
          it->height = g_key_file_get_integer (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_HEIGHT, nullptr);*/
          it->working_dir = terminal_util_key_file_get_string_unescape (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_WORKING_DIRECTORY, nullptr);
          it->title = g_key_file_get_string (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_TITLE, nullptr);

          if (g_key_file_has_key (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_COMMAND, nullptr) &&
              !(it->exec_argv = terminal_util_key_file_get_argv (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_COMMAND, nullptr, error)))
            {
              have_error = TRUE;
              break;
            }
        }

      g_free (active_terminal);
      g_strfreev (tab_groups);

      if (have_error)
        break;
    }

  g_strfreev (groups);

  if (have_error)
    {
      g_list_free_full (initial_windows, (GDestroyNotify) initial_window_free);
      return FALSE;
    }

  options->initial_windows = g_list_concat (options->initial_windows, initial_windows);

  return TRUE;
}

/**
 * terminal_options_ensure_window:
 * @options:
 *
 * Ensure that @options will contain at least one window to open.
 */
void
terminal_options_ensure_window (TerminalOptions *options)
{
  terminal_options_ensure_schema_source(options);
  gs_unref_object auto global_settings =
    terminal_g_settings_new(nullptr, // default backend
                            options->schema_source,
                            TERMINAL_SETTING_SCHEMA);

  gs_free char *mode_str = g_settings_get_string (global_settings,
                                                  TERMINAL_SETTING_NEW_TERMINAL_MODE_KEY);

  gboolean implicit_if_first_window = g_str_equal (mode_str, "tab");
  ensure_top_window (options, implicit_if_first_window);
}

/**
 * terminal_options_free:
 * @options:
 *
 * Frees @options.
 */
void
terminal_options_free (TerminalOptions *options)
{
  g_list_free_full (options->initial_windows, (GDestroyNotify) initial_window_free);

  g_free (options->default_role);
  g_free (options->default_geometry);
  g_free (options->default_working_dir);
  g_free (options->default_title);
  g_free (options->default_profile);

  g_strfreev (options->exec_argv);

  g_free (options->server_unique_name);
  g_free (options->parent_screen_object_path);

  g_free (options->display_name);
  g_free (options->startup_id);
  g_free (options->activation_token);
  g_free (options->server_app_id);

  g_free (options->sm_client_id);
  g_free (options->sm_config_prefix);

  g_clear_object (&options->profiles_list);
  g_clear_pointer (&options->schema_source, g_settings_schema_source_unref);

  g_free (options);
}

static GOptionContext *
get_goption_context (TerminalOptions *options)
{
  const GOptionEntry global_unique_goptions[] = {
    {
      "app-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)option_app_id_callback,
      "Server application ID",
      "ID"
    },
    {
      "disable-factory",
      0,
      G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)unsupported_option_fatal_callback,
      N_("Do not register with the activation nameserver, do not re-use an active terminal"),
      nullptr
    },
    {
      "load-config",
      0,
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      (void*)option_load_config_cb,
      N_("Load a terminal configuration file"),
      N_("FILE")
    },
    {
      "save-config",
      0,
      G_OPTION_FLAG_FILENAME | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)unsupported_option_callback,
      nullptr, nullptr
    },
    {
      "no-environment",
      0,
      0,
      G_OPTION_ARG_NONE,
      &options->no_environment,
      N_("Do not pass the environment"),
      nullptr
    },
    {
      "preferences",
      0,
      0,
      G_OPTION_ARG_NONE,
      &options->show_preferences,
      N_("Show preferences window"),
      nullptr
    },
    {
      "print-environment",
      'p',
      0,
      G_OPTION_ARG_NONE,
      &options->print_environment,
      N_("Print environment variables to interact with the terminal"),
      nullptr
    },
    {
      "version",
      0,
      G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)option_version_cb,
      nullptr,
      nullptr
    },
    {
      "verbose",
      'v',
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_verbosity_cb,
      N_("Increase diagnostic verbosity"),
      nullptr
    },
    {
      "quiet",
      'q',
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_verbosity_cb,
      N_("Suppress output"),
      nullptr
    },
    { nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
  };

  const GOptionEntry global_multiple_goptions[] = {
    {
      "window",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_window_callback,
      N_("Open a new window containing a tab with the default profile"),
      nullptr
    },
    {
      "tab",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_tab_callback,
      N_("Open a new tab in the last-opened window with the default profile"),
      nullptr
    },
    { nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
  };

  const GOptionEntry window_goptions[] = {
    {
      "show-menubar",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_show_menubar_callback,
      N_("Turn on the menubar"),
      nullptr
    },
    {
      "hide-menubar",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_hide_menubar_callback,
      N_("Turn off the menubar"),
      nullptr
    },
    {
      "maximize",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_maximize_callback,
      N_("Maximize the window"),
      nullptr
    },
    {
      "full-screen",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_fullscreen_callback,
      N_("Full-screen the window"),
      nullptr
    },
    {
      "geometry",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      (void*)option_geometry_callback,
      N_("Set the window size; for example: 80x24, or 80x24+200+200 (COLSxROWS+X+Y)"),
      N_("GEOMETRY")
    },
    {
      "role",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      (void*)option_role_callback,
      N_("Set the window role"),
      N_("ROLE")
    },
    {
      "active",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_active_callback,
      N_("Set the last specified tab as the active one in its window"),
      nullptr
    },
    { nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
  };

  const GOptionEntry terminal_goptions[] = {
    {
      "command",
      'e',
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      (void*)option_command_callback,
      N_("Execute the argument to this option inside the terminal"),
      nullptr
    },
    {
      "profile",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      (void*)option_profile_cb,
      N_("Use the given profile instead of the default profile"),
      N_("PROFILE-NAME")
    },
    {
      "title",
      't',
      0,
      G_OPTION_ARG_CALLBACK,
      (void*)option_title_callback,
      N_("Set the initial terminal title"),
      N_("TITLE")
    },
    {
      "working-directory",
      0,
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      (void*)option_working_directory_callback,
      N_("Set the working directory"),
      N_("DIRNAME")
    },
    {
      "wait",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      (void*)option_wait_cb,
      N_("Wait until the child exits"),
      nullptr
    },
    {
      "fd",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      (void*)option_pass_fd_cb,
      N_("Forward file descriptor"),
      /* FD = file descriptor */
      N_("FD")
    },
    {
      "zoom",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      (void*)option_zoom_callback,
      N_("Set the terminal’s zoom factor (1.0 = normal size)"),
      N_("ZOOM")
    },
    { nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
  };

  const GOptionEntry internal_goptions[] = {  
    {
      "profile-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)option_profile_id_cb,
      nullptr, nullptr
    },
    {
      "window-with-profile",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)option_window_callback,
      nullptr, nullptr
    },
    {
      "tab-with-profile",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)option_tab_callback,
      nullptr, nullptr
    },
    {
      "window-with-profile-internal-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)option_window_callback,
      nullptr, nullptr
    },
    {
      "tab-with-profile-internal-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)option_tab_callback,
      nullptr, nullptr
    },
    {
      "default-working-directory",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_FILENAME,
      &options->default_working_dir,
      nullptr, nullptr,
    },
    {
      "use-factory",
      0,
      G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      (void*)unsupported_option_callback,
      nullptr, nullptr
    },
    {
      "startup-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &options->startup_id,
      nullptr,
      nullptr
    },
    {
      "activation-token",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &options->activation_token,
      nullptr,
      nullptr
    },
    { nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
  };

  const GOptionEntry smclient_goptions[] = {
    { "sm-client-disable",    0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &options->sm_client_disable,    nullptr, nullptr },
    { "sm-client-state-file", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, (void*)option_load_config_cb, nullptr, nullptr },
    { "sm-client-id",         0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,   &options->sm_client_id,         nullptr, nullptr },
    { "sm-disable",           0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &options->sm_client_disable,    nullptr, nullptr },
    { "sm-config-prefix",     0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,   &options->sm_config_prefix,     nullptr, nullptr },
    { nullptr }
  };

  GOptionContext *context;
  GOptionGroup *group;
  gs_free char *parameter;

  parameter = g_strdup_printf ("[-- %s …]", _("COMMAND"));
  context = g_option_context_new (parameter);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_ignore_unknown_options (context, FALSE);

  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  group = g_option_group_new ("gnome-terminal",
                              N_("GNOME Terminal Emulator"),
                              N_("Show GNOME Terminal options"),
                              options,
                              nullptr);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, global_unique_goptions);
  g_option_group_add_entries (group, internal_goptions);
  g_option_group_set_parse_hooks (group, nullptr, digest_options_callback);
  g_option_context_set_main_group (context, group);

  group = g_option_group_new ("terminal",
                              N_("Options to open new windows or terminal tabs; more than one of these may be specified:"),
                              N_("Show terminal options"),
                              options,
                              nullptr);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, global_multiple_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("window-options",
                              N_("Window options; if used before the first --window or --tab argument, sets the default for all windows:"),
                              N_("Show per-window options"),
                              options,
                              nullptr);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, window_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("terminal-options",
                              N_("Terminal options; if used before the first --window or --tab argument, sets the default for all terminals:"),
                              N_("Show per-terminal options"),
                              options,
                              nullptr);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, terminal_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("sm-client", "", "", options, nullptr);
  g_option_group_add_entries (group, smclient_goptions);
  g_option_context_add_group (context, group);

  return context;
}
