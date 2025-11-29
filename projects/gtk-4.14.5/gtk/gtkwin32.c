/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdk/gdk.h"

#include "gtkprivate.h"

#define STRICT
#include <windows.h>
#include <commctrl.h>
#undef STRICT

extern IMAGE_DOS_HEADER __ImageBase;

static inline HMODULE
this_module (void)
{
  return (HMODULE) &__ImageBase;
}

/* In practice, resulting DLL will have manifest resource under index 2.
 * Fall back to that value if we can't find resource index programmatically.
 */
#define EMPIRIC_MANIFEST_RESOURCE_INDEX 2

static BOOL CALLBACK
find_first_manifest (HMODULE  module_handle,
                     LPCSTR   resource_type,
                     LPSTR    resource_name,
                     LONG_PTR user_data)
{
  LPSTR *result_name = (LPSTR *) user_data;

  if (resource_type == RT_MANIFEST)
    {
      if (IS_INTRESOURCE (resource_name))
        *result_name = resource_name;
      else
        *result_name = g_strdup (resource_name);
      return FALSE;
    }
  return TRUE;
}

/*
 * Grabs the first manifest it finds in libgtk3 (which is expected to be the
 * common-controls-6.0.0.0 manifest we embedded to enable visual styles),
 * uses it to create a process-default activation context, activates that
 * context, loads up the library passed in @dllname, then deactivates and
 * releases the context.
 *
 * In practice this is used to force system DLLs (like comdlg32) to be
 * loaded as if the application had the same manifest as libgtk3
 * (otherwise libgtk3 manifest only affests libgtk3 itself).
 * This way application does not need to have a manifest or to link
 * against comctl32.
 *
 * Note that loaded library handle leaks, so only use this function in
 * g_once_init_enter (leaking once is OK, Windows will clean up after us).
 */
void
_gtk_load_dll_with_libgtk3_manifest (const char *dll_name)
{
  HANDLE activation_ctx_handle;
  ACTCTXA activation_ctx_descriptor;
  ULONG_PTR activation_cookie;
  LPSTR resource_name;
  BOOL activated;
  DWORD error_code;

  resource_name = NULL;
  EnumResourceNames (this_module (), RT_MANIFEST, find_first_manifest,
                     (LONG_PTR) &resource_name);

  if (resource_name == NULL)
    resource_name = MAKEINTRESOURCEA (EMPIRIC_MANIFEST_RESOURCE_INDEX);

  memset (&activation_ctx_descriptor, 0, sizeof (activation_ctx_descriptor));
  activation_ctx_descriptor.cbSize = sizeof (activation_ctx_descriptor);
  activation_ctx_descriptor.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID |
                                      ACTCTX_FLAG_HMODULE_VALID |
                                      ACTCTX_FLAG_SET_PROCESS_DEFAULT;
  activation_ctx_descriptor.hModule = this_module ();
  activation_ctx_descriptor.lpResourceName = resource_name;
  activation_ctx_handle = CreateActCtx (&activation_ctx_descriptor);
  error_code = GetLastError ();

  if (activation_ctx_handle == INVALID_HANDLE_VALUE &&
      error_code != ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET)
    g_warning ("Failed to CreateActCtx for module %p, resource %p: %lu",
               this_module (), resource_name, GetLastError ());
  else if (error_code != ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET)
    {
      activation_cookie = 0;
      activated = ActivateActCtx (activation_ctx_handle, &activation_cookie);

      if (!activated)
        g_warning ("Failed to ActivateActCtx: %lu", GetLastError ());

      LoadLibraryA (dll_name);

      if (activated && !DeactivateActCtx (0, activation_cookie))
        g_warning ("Failed to DeactivateActCtx: %lu", GetLastError ());

      ReleaseActCtx (activation_ctx_handle);
    }

  if (!IS_INTRESOURCE (resource_name))
    g_free (resource_name);
}

const char *
_gtk_get_libdir (void)
{
  static char *gtk_libdir = NULL;
  if (gtk_libdir == NULL)
    {
      char *root = g_win32_get_package_installation_directory_of_module (this_module ());
      char *slash = strrchr (root, '\\');
      if (slash != NULL &&
          g_ascii_strcasecmp (slash + 1, ".libs") == 0)
        gtk_libdir = g_strdup (GTK_LIBDIR);
      else
        gtk_libdir = g_build_filename (root, "lib", NULL);
      g_free (root);
    }

  return gtk_libdir;
}

const char *
_gtk_get_localedir (void)
{
  static char *gtk_localedir = NULL;
  if (gtk_localedir == NULL)
    {
      const char *p;
      char *root, *temp;

      /* GTK_LOCALEDIR ends in either /lib/locale or
       * /share/locale. Scan for that slash.
       */
      p = GTK_LOCALEDIR + strlen (GTK_LOCALEDIR);
      while (*--p != '/')
        ;
      while (*--p != '/')
        ;

      root = g_win32_get_package_installation_directory_of_module (this_module ());
      temp = g_build_filename (root, p, NULL);
      g_free (root);

      /* gtk_localedir is passed to bindtextdomain() which isn't
       * UTF-8-aware.
       */
      gtk_localedir = g_win32_locale_filename_from_utf8 (temp);
      g_free (temp);
    }
  return gtk_localedir;
}

const char *
_gtk_get_datadir (void)
{
  static char *gtk_datadir = NULL;
  if (gtk_datadir == NULL)
    {
      char *root = g_win32_get_package_installation_directory_of_module (this_module ());
      gtk_datadir = g_build_filename (root, "share", NULL);
      g_free (root);
    }

  return gtk_datadir;
}

const char *
_gtk_get_sysconfdir (void)
{
  static char *gtk_sysconfdir = NULL;
  if (gtk_sysconfdir == NULL)
    {
      char *root = g_win32_get_package_installation_directory_of_module (this_module ());
      gtk_sysconfdir = g_build_filename (root, "etc", NULL);
      g_free (root);
    }

  return gtk_sysconfdir;
}

const char *
_gtk_get_data_prefix (void)
{
  static char *gtk_data_prefix = NULL;
  if (gtk_data_prefix == NULL)
    gtk_data_prefix = g_win32_get_package_installation_directory_of_module (this_module ());

  return gtk_data_prefix;
}
