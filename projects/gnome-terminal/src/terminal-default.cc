/*
 * Copyright © 2008, 2011, 2025 Christian Persch
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

#include <fcntl.h>

#include "terminal-default.hh"

#include "terminal-debug.hh"
#include "terminal-defines.hh"
#include "terminal-libgsystem.hh"

#define XTE_CONFIG_DIRNAME  "xdg-terminals"
#define XTE_CONFIG_FILENAME "xdg-terminals.list"

#define NEWLINE '\n'
#define DOT_DESKTOP ".desktop"
#define TERMINAL_DESKTOP_FILENAME TERMINAL_APPLICATION_ID DOT_DESKTOP

static char**
get_desktops_lc(void)
{
  auto const desktop = g_getenv("XDG_CURRENT_DESKTOP");
  if (!desktop)
    return nullptr;

  auto strv = g_strsplit(desktop, G_SEARCHPATH_SEPARATOR_S, -1);
  if (!strv)
    return nullptr;

  for (auto p = strv; *p; ++p) {
    gs_free auto str = *p;
    *p = g_ascii_strdown(str, -1);
  }

  return strv;
}

static bool
xte_data_check_one(char const* file,
                   bool full)
{
  if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Desktop file \"%s\" does not exist.\n",
                          file);
    return false;
  }

  if (!full)
    return true;

  gs_free_error GError* error = nullptr;
  gs_unref_key_file auto kf = g_key_file_new();
  if (!g_key_file_load_from_file(kf,
                                 file,
                                 GKeyFileFlags(G_KEY_FILE_NONE),
                                 &error)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to load  \"%s\" as keyfile: %s\n",
                          file, error->message);

    return false;
  }

  if (!g_key_file_has_group(kf, G_KEY_FILE_DESKTOP_GROUP)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Keyfile file \"%s\" is not a desktop file.\n",
                          file);
    return false;
  }

  // As per the XDG desktop entry spec, the (optional) TryExec key contains
  // the name of an executable that can be used to determine if the programme
  // is actually present.
  gs_free auto try_exec = g_key_file_get_string(kf,
                                                G_KEY_FILE_DESKTOP_GROUP,
                                                G_KEY_FILE_DESKTOP_KEY_TRY_EXEC,
                                                nullptr);
  if (try_exec && try_exec[0]) {
    // TryExec may be an abolute path, or be searched in $PATH
    gs_free char* exec_path = nullptr;
    if (g_path_is_absolute(try_exec))
      exec_path = g_strdup(try_exec);
    else
      exec_path = g_find_program_in_path(try_exec);

    auto const exists = exec_path != nullptr &&
      g_file_test(exec_path, GFileTest(G_FILE_TEST_IS_EXECUTABLE));

    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Desktop file \"%s\" is %sinstalled (TryExec).\n",
                          file, exists ? "" : "not ");

    if (!exists)
      return false;
  } else {
    // TryExec is not present. We could fall back to parsing the Exec
    // key and look if its first argument points to an executable that
    // exists on the system, but that may also fail if the desktop file
    // is DBusActivatable=true in which case we would need to find
    // out if the D-Bus service corresponding to the name of the desktop
    // file (without the .desktop extension) is activatable.

    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Desktop file \"%s\" has no TryExec field.\n",
                          file);
  }

  return true;
}

static bool
xte_data_check(char const* name,
               bool full)
{
  { // Legacy x-t-e spec
    gs_free auto path = g_build_filename(g_get_user_data_dir(),
                                         XTE_CONFIG_DIRNAME,
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;
  }
  {
    gs_free auto path = g_build_filename(g_get_user_data_dir(),
                                         "applications",
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;
  }
  {
    gs_free auto path = g_build_filename(g_get_user_data_dir(),
                                         "flatpak",
                                         "exports",
                                         "share",
                                         "applications",
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;
  }
  if (!g_str_equal(TERM_PREFIX, "/usr/local")) {
    gs_free auto path = g_build_filename(TERM_PREFIX,
                                         "local",
                                         "share",
                                         XTE_CONFIG_DIRNAME,
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;

    gs_free auto path2 = g_build_filename(TERM_PREFIX,
                                          "local",
                                          "share",
                                          "applications",
                                          name,
                                         nullptr);
    if (xte_data_check_one(path2, full))
      return true;
  }
  {
    gs_free auto path = g_build_filename(TERM_DATADIR,
                                         XTE_CONFIG_DIRNAME,
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;
  }
  {
    gs_free auto path = g_build_filename(TERM_DATADIR,
                                         "applications",
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;
  }
  {
    gs_free auto path = g_build_filename("/var",
                                         "lib",
                                         "flatpak",
                                         "exports",
                                         "share",
                                         "applications",
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;
  }
  if (!g_str_equal(TERM_PREFIX, "/usr")) {
    gs_free auto path = g_build_filename("/usr", "share",
                                         XTE_CONFIG_DIRNAME,
                                         name,
                                         nullptr);
    if (xte_data_check_one(path, full))
      return true;

    gs_free auto path2 = g_build_filename("/usr",
                                          "share",
                                          "applications",
                                          name,
                                          nullptr);
    if (xte_data_check_one(path2, full))
      return true;
  }

  return false;
}

static bool
xte_data_ensure(char const* desktop_id)
{
  if (xte_data_check(desktop_id, false))
    return true;

  // If we get here, there wasn't a desktop file in any of the paths. Install
  // a symlink to the system-installed desktop file into the user path.

  gs_free auto user_dir = g_build_filename(g_get_user_data_dir(),
                                           XTE_CONFIG_DIRNAME,
                                           nullptr);
  if (g_mkdir_with_parents(user_dir, 0700) != 0 &&
      errno != EEXIST) {
    auto const errsv = errno;
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to create directory %s: %s\n",
                          user_dir, g_strerror(errsv));
    return false;
  }

  gs_free auto link_path = g_build_filename(user_dir,
                                            desktop_id,
                                            nullptr);
  gs_free auto target_path = g_build_filename(TERM_DATADIR,
                                              "applications",
                                              desktop_id,
                                              nullptr);

  auto const r = symlink(target_path, link_path);
  if (r != -1) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Installed symlink %s -> %s\n",
                          link_path, target_path);

  } else {
    auto const errsv = errno;
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to create symlink %s: %s\n",
                          link_path, g_strerror(errsv));
  }

  return r != -1;
}

static char**
xte_config_read(char const* path,
                GError** error)
{
  _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                        "Reading x-t-e config file \"%s\"\n",
                        path);

  gs_close_fd auto fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd == -1)
    return nullptr;

  // This is a small config file, so shouldn't be any bigger than this.
  // If it is bigger, we'll discard the rest. That's why we're not using
  // g_file_get_contents() here.
  char buf[8192];
  auto r = ssize_t{};
  do {
    r = read(fd, buf, sizeof(buf) - 1); // reserve one byte in buf
  } while (r == -1 && errno == EINTR);
  if (r < 0)
    return nullptr;

  buf[r] = '\0'; // NUL terminator; note that r < sizeof(buf)

  auto lines = g_strsplit_set(buf, "\r\n", -1);
  if (!lines)
    return nullptr;

  for (auto i = 0; lines[i]; ++i)
    lines[i] = g_strstrip(lines[i]);

  return lines;
}

static bool
xte_config_rewrite(char const* path,
                   char const* desktop_id)
{
  gs_free_gstring auto str = g_string_sized_new(1024);
  g_string_append(str, desktop_id);
  g_string_append_c(str, NEWLINE);

  gs_strfreev auto lines = xte_config_read(path, nullptr);
  if (lines) {
    for (auto i = 0; lines[i]; ++i) {
      if (lines[i][0] == '\0')
        continue;
      if (strcmp(lines[i], desktop_id) == 0)
        continue;

      g_string_append(str, lines[i]);
      g_string_append_c(str, NEWLINE);
    }
  }

  gs_free_error GError* error = nullptr;
  auto const r = g_file_set_contents(path, str->str, str->len, &error);
  if (!r) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to rewrite XTE config %s: %s\n",
                          path, error->message);
  }

  return r;
}

static void
xte_config_rewrite(char const* desktop_id)
{
  auto const user_dir = g_get_user_config_dir();
  if (g_mkdir_with_parents(user_dir, 0700) != 0 &&
      errno != EEXIST) {
    auto const errsv = errno;
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to create directory %s: %s\n",
                          user_dir, g_strerror(errsv));
   // Nothing to do if we can't even create the directory
    return;
  }

  // Install as default for all current desktops
  gs_strfreev auto desktops = get_desktops_lc();
  if (desktops) {
    for (auto i = 0; desktops[i]; ++i) {
      gs_free auto name = g_strdup_printf("%s-" XTE_CONFIG_FILENAME,
                                          desktops[i]);
      gs_free auto path = g_build_filename(user_dir, name, nullptr);

      xte_config_rewrite(path, desktop_id);
    }
  }

  // Install as non-desktop specific default too
  gs_free auto path = g_build_filename(user_dir, XTE_CONFIG_FILENAME, nullptr);
  xte_config_rewrite(path, desktop_id);
}

static bool
xte_config_is_foreign(char const* name,
                      char const* native_name)
{
  return !g_str_equal(name, native_name);
}

static char*
xte_config_get_default_for_path(char const* path,
                                char const* native_name)
{
  gs_strfreev auto lines = xte_config_read(path, nullptr);
  if (!lines)
    return nullptr;

  // A terminal is the default if it's the first non-comment line in the file
  for (auto i = 0; lines[i]; ++i) {
    auto const line = lines[i];
    if (!line[0] || line[0] == '#')
      continue;

    // If a foreign terminal is default, check whether it is actually installed.
    // (We always ensure our own desktop file exists.)
    if (xte_config_is_foreign(line, native_name) &&
        !xte_data_check(line, true)) {
      _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                            "Default entry \"%s\" from config \"%s\" is not installed, skipping.\n",
                            line, path);
      return nullptr;
    }

    return g_strdup(line);
  }

  return nullptr;
}

static char*
xte_config_get_default_for_path_and_desktops(char const* base_path,
                                             char const* const* desktops,
                                             char const* native_name)
{
  if (desktops) {
    for (auto i = 0; desktops[i]; ++i) {
      gs_free auto name = g_strdup_printf("%s-" XTE_CONFIG_FILENAME,
                                          desktops[i]);
      gs_free auto path = g_build_filename(base_path, name, nullptr);
      if (auto term = xte_config_get_default_for_path(path, native_name))
        return term;
    }
  }

  gs_free auto sys_path = g_build_filename(base_path, XTE_CONFIG_FILENAME, nullptr);
  if (auto term = xte_config_get_default_for_path(sys_path, native_name))
    return term;

  return nullptr;
}

static char*
xte_config_get_default(char const* native_name)
{
  gs_strfreev auto desktops = get_desktops_lc();
  auto const user_dir = g_get_user_config_dir();
  if (auto term = xte_config_get_default_for_path_and_desktops(user_dir, desktops, native_name))
    return term;
  if (auto term = xte_config_get_default_for_path_and_desktops("/etc/xdg", desktops, native_name))
    return term;
  if (auto term = xte_config_get_default_for_path_and_desktops("/usr/etc/xdg", desktops, native_name))
    return term;

  return nullptr;
}

static bool
xte_config_is_default(char const* desktop_id,
                      bool* set = nullptr)
{
  gs_free auto term = xte_config_get_default(desktop_id);

  auto const is_default = term && g_str_equal(term, desktop_id);
  if (set)
    *set = term != nullptr;
  return is_default;
}

gboolean
terminal_is_default(void)
{
  auto const desktop_id = TERMINAL_DESKTOP_FILENAME;
  auto set = false;
  auto const is_default = xte_config_is_default(desktop_id, &set);
  if (!set) {
    // No terminal is default yet, so we claim the default.
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "No default terminal, claiming default.\n");
    return terminal_make_default();
  }

  if (is_default) {
    // If we're the default terminal, ensure our desktop file is installed
    // in the right location.
    xte_data_ensure(desktop_id);
  }

  return is_default;
}

gboolean
terminal_make_default(void)
{
  auto const desktop_id = TERMINAL_DESKTOP_FILENAME;

  xte_config_rewrite(desktop_id);
  xte_data_ensure(desktop_id);

  return xte_config_is_default(desktop_id);
}

#ifdef TERMINAL_DEFAULT_MAIN

static int
usage(char const* argv0) noexcept
{
  g_printerr("Usage: %s [--debug] [get|set] [DESKTOP_ID]\n", argv0);
  return 1;
}

static void
show_default(char const* desktop_id)
{
  g_printerr("Reading default terminal...\n");
  gs_free auto term = xte_config_get_default(desktop_id);
  if (term) {
    g_printerr("Default terminal: %s\n", term);
  } else {
    g_printerr("Default terminal not set\n");
  }
}

int
main(int argc,
     char* argv[])
{
  if (argc < 2)
    return usage(argv[0]);

  auto idx = 1;
  if (g_str_equal(argv[idx], "--debug")) {
    g_setenv("GNOME_TERMINAL_DEBUG", "default", true);
    _terminal_debug_init();
    ++idx;
  }

  if (idx >= argc)
    return usage(argv[0]);

  auto const verb = argv[idx++];
  auto const desktop_id = idx < argc ? argv[idx] : TERMINAL_DESKTOP_FILENAME;

  if (g_str_equal(verb, "get")) {
    show_default(desktop_id);
  } else if (g_str_equal(verb, "set")) {
    g_printerr("Setting %s as default terminal...\n", desktop_id);
    xte_config_rewrite(desktop_id);
    // xte_data_ensure(desktop_id);
    show_default(desktop_id);
  } else {
    return usage(argv[0]);
  }

  return 0;
}

#endif // TERMINAL_DEFAULT_MAIN
