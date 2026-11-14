/* -*- mode: C; c-file-style: "linux" -*- */
/* GdkPixbuf library
 * queryloaders.c:
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Matthias Clasen <maclas@gmx.de>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gdk-pixbuf/gdk-pixbuf.h"
#include "gdk-pixbuf/gdk-pixbuf-private.h"

#ifdef USE_LA_MODULES
#define SOEXT ".la"
#else
#define SOEXT ("." G_MODULE_SUFFIX)
#endif
#define SOEXT_LEN (strlen (SOEXT))

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#ifdef OS_DARWIN
#include <mach-o/dyld.h>
#endif

static void
print_escaped (GString *contents, const char *str)
{
        gchar *tmp = g_strescape (str, "");
        g_string_append_printf (contents, "\"%s\" ", tmp);
        g_free (tmp);
}

static int
loader_sanity_check (const char *path, GdkPixbufFormat *info, GdkPixbufModule *vtable)
{
        const GdkPixbufModulePattern *pattern;
        const char *error = "";

        for (pattern = info->signature; pattern->prefix; pattern++)
        {
                int prefix_len = strlen (pattern->prefix);
                if (prefix_len == 0)
                {
                        error = "empty pattern";

                        goto error;
                }
                if (pattern->mask)
                {
                        int mask_len = strlen (pattern->mask);
                        if (mask_len != prefix_len)
                        {
                                error = "mask length mismatch";

                                goto error;
                        }
                        if (strspn (pattern->mask, " !xzn*") < mask_len)
                        {
                                error = "bad char in mask";

                                goto error;
                        }
                }
        }

        if (!vtable->load && !vtable->begin_load && !vtable->load_animation)
        {
                error = "no load method implemented";

                goto error;
        }

        if (vtable->begin_load && (!vtable->stop_load || !vtable->load_increment))
        {
                error = "incremental loading support incomplete";

                goto error;
        }

        if ((info->flags & GDK_PIXBUF_FORMAT_WRITABLE) && !(vtable->save || vtable->save_to_callback))
        {
                error = "loader claims to support saving but doesn't implement save";
                goto error;
        }

        return 1;

 error:
        g_fprintf (stderr, "Loader sanity check failed for %s: %s\n",
                   path, error);

        return 0;
}

#ifdef GDK_PIXBUF_RELOCATABLE

/* Based on gdk_pixbuf_get_toplevel () */
static gchar *
get_toplevel (void)
{
        static gchar *toplevel = NULL;

        if (toplevel == NULL) {
#if defined (G_OS_WIN32)
                toplevel = g_win32_get_package_installation_directory_of_module (NULL);
#elif defined(OS_DARWIN)
                char pathbuf[PATH_MAX + 1];
                uint32_t bufsize = sizeof (pathbuf);
                gchar *bin_dir;

                _NSGetExecutablePath (pathbuf, &bufsize);
                bin_dir = g_dirname (pathbuf);
                toplevel = g_build_path (G_DIR_SEPARATOR_S, bin_dir, "..", NULL);
                g_free (bin_dir);
#elif defined (OS_LINUX) || defined (__MINGW32__)
                gchar *exe_path, *bin_dir;

                exe_path = g_file_read_link ("/proc/self/exe", NULL);
                bin_dir = g_dirname (exe_path);
                toplevel = g_build_path (G_DIR_SEPARATOR_S, bin_dir, "..", NULL);
                g_free (exe_path);
                g_free (bin_dir);
#else
#error "Relocations not supported for this platform"
#endif
        }
        return toplevel;
}

/* Returns the relative path or NULL; transfer full */
static gchar *
get_relative_path (const gchar *parent, const gchar *descendant)
{
        GFile *parent_file, *descendant_file;
        char *relative_path;

        parent_file = g_file_new_for_path (parent);
        descendant_file = g_file_new_for_path (descendant);
        relative_path = g_file_get_relative_path (parent_file, descendant_file);
        g_object_unref (parent_file);
        g_object_unref (descendant_file);

        return relative_path;
}

#endif  /* GDK_PIXBUF_RELOCATABLE */

static void
write_loader_info (GString *contents, const char *path, GdkPixbufFormat *info)
{
        const GdkPixbufModulePattern *pattern;
        char **mime;
        char **ext;
        gchar *module_path = NULL, *escaped_path;

#ifdef GDK_PIXBUF_RELOCATABLE
        module_path = get_relative_path (get_toplevel (), path);
#endif

        if (module_path == NULL) {
                module_path = g_strdup (path);
        }

        escaped_path = g_strescape (module_path, "");
        g_string_append_printf (contents, "\"%s\"\n", escaped_path);
        g_free (module_path);
        g_free (escaped_path);

        g_string_append_printf (contents, "\"%s\" %u \"%s\" \"%s\" \"%s\"\n",
                  info->name,
                  info->flags,
                  info->domain ? info->domain : GETTEXT_PACKAGE,
                  info->description,
                  info->license ? info->license : "");
        for (mime = info->mime_types; *mime; mime++) {
                g_string_append_printf (contents, "\"%s\" ", *mime);
        }
        g_string_append (contents, "\"\"\n");
        for (ext = info->extensions; *ext; ext++) {
                g_string_append_printf (contents, "\"%s\" ", *ext);
        }
        g_string_append (contents, "\"\"\n");
        for (pattern = info->signature; pattern->prefix; pattern++) {
                print_escaped (contents, pattern->prefix);
                print_escaped (contents, pattern->mask ? (const char *)pattern->mask : "");
                g_string_append_printf (contents, "%d\n", pattern->relevance);
        }
        g_string_append_c (contents, '\n');
}

static void
query_module (GString *contents, const char *dir, const char *file)
{
        char *path;
        GModule *module;
        void                    (*fill_info)     (GdkPixbufFormat *info);
        void                    (*fill_vtable)   (GdkPixbufModule *module);
        gpointer fill_info_ptr;
        gpointer fill_vtable_ptr;

        if (g_path_is_absolute (file))
                path = g_strdup (file);
        else
                path = g_build_filename (dir, file, NULL);

        /* Fil-C port: g_module_open (path, 0) resolves to
         * dlopen (path, RTLD_LAZY | RTLD_GLOBAL).  gdk-pixbuf loader modules
         * all export the same entry-point names ("fill_info", "fill_vtable"),
         * and the Fil-C dlsym searches the global scope in load order instead
         * of preferring the given handle, so with two RTLD_GLOBAL modules
         * loaded, dlsym (svg_handle, "fill_info") resolved to the ANI
         * module's fill_info and the generated loaders.cache associated every
         * loader path with the first module's format info (making
         * gdk_pixbuf_new_from_file unable to recognize anything but the
         * first-loaded format).  Load the modules RTLD_LOCAL like the runtime
         * loader in gdk-pixbuf-io.c does: with LOCAL, dlsym on a handle
         * correctly resolves within that module. */
        module = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
        if (module &&
            g_module_symbol (module, "fill_info", &fill_info_ptr) &&
            g_module_symbol (module, "fill_vtable", &fill_vtable_ptr)) {
                GdkPixbufFormat *info;
                GdkPixbufModule *vtable;

#ifdef G_OS_WIN32
                /* Replace backslashes in path with forward slashes, so that
                 * it reads in without problems.
                 */
                {
                        char *p = path;
                        while (*p) {
                                if (*p == '\\')
                                        *p = '/';
                                p++;
                        }
                }
#endif
                info = g_new0 (GdkPixbufFormat, 1);
                vtable = g_new0 (GdkPixbufModule, 1);

                vtable->module = module;

                fill_info = fill_info_ptr;
                fill_vtable = fill_vtable_ptr;

                (*fill_info) (info);
                (*fill_vtable) (vtable);

                if (loader_sanity_check (path, info, vtable))
                        write_loader_info (contents, path, info);

                g_free (info);
                g_free (vtable);
        }
        else {
                if (module == NULL)
                        g_fprintf (stderr, "g_module_open() failed for %s: %s\n", path,
                                   g_module_error());
                else
                        g_fprintf (stderr, "Cannot load loader %s\n", path);
        }
        if (module)
                g_module_close (module);
        g_free (path);
}

#if defined(G_OS_WIN32) && defined(GDK_PIXBUF_RELOCATABLE)

static char *
get_libdir (void)
{
  static char *libdir = NULL;

  if (libdir == NULL)
          libdir = g_build_filename (get_toplevel (), "lib", NULL);

  return libdir;
}

#undef GDK_PIXBUF_LIBDIR
#define GDK_PIXBUF_LIBDIR get_libdir()

#endif

static gchar *
gdk_pixbuf_get_module_file (void)
{
        gchar *result = g_strdup (g_getenv ("GDK_PIXBUF_MODULE_FILE"));

        if (!result)
                result = g_build_filename (GDK_PIXBUF_LIBDIR, "gdk-pixbuf-2.0", GDK_PIXBUF_BINARY_VERSION, "loaders.cache", NULL);

        return result;
}

int main (int argc, char **argv)
{
        gint i;
        const gchar *prgname;
        GString *contents;
        gchar *cache_file = NULL;
        gint first_file = 1;
        GFile *pixbuf_libdir_file;
        gchar *pixbuf_libdir;

#ifdef G_OS_WIN32
        gchar *libdir;
        GFile *pixbuf_prefix_file;
        gchar *pixbuf_prefix;
#endif

        /* An intermediate GFile here will convert all the path separators
         * to the right one used by the platform
         */
        pixbuf_libdir_file = g_file_new_for_path (PIXBUF_LIBDIR);
        pixbuf_libdir = g_file_get_path (pixbuf_libdir_file);
        g_object_unref (pixbuf_libdir_file);

#ifdef G_OS_WIN32
        pixbuf_prefix_file = g_file_new_for_path (GDK_PIXBUF_PREFIX);
        pixbuf_prefix = g_file_get_path (pixbuf_prefix_file);
        g_object_unref (pixbuf_prefix_file);

        if (g_ascii_strncasecmp (pixbuf_libdir, pixbuf_prefix, strlen (pixbuf_prefix)) == 0 &&
            G_IS_DIR_SEPARATOR (pixbuf_libdir[strlen (pixbuf_prefix)])) {
                gchar *runtime_prefix;
                gchar *slash;

                /* pixbuf_prefix is a prefix of pixbuf_libdir, as it
                 * normally is. Replace that prefix in pixbuf_libdir
                 * with the installation directory on this machine.
                 * We assume this invokation of
                 * gdk-pixbuf-query-loaders is run from either a "bin"
                 * subdirectory of the installation directory, or in
                 * the installation directory itself.
                 */
                wchar_t fn[1000];
                GetModuleFileNameW (NULL, fn, G_N_ELEMENTS (fn));
                runtime_prefix = g_utf16_to_utf8 (fn, -1, NULL, NULL, NULL);
                slash = strrchr (runtime_prefix, '\\');
                *slash = '\0';
                slash = strrchr (runtime_prefix, '\\');
                /* If running from some weird location, or from the
                 * build directory (either in the .libs folder where
                 * libtool places the real executable when using a
                 * wrapper, or directly from the gdk-pixbuf folder),
                 * use the compile-time libdir.
                 */
                if (slash == NULL ||
                    g_ascii_strcasecmp (slash + 1, ".libs") == 0 ||
                    g_ascii_strcasecmp (slash + 1, "gdk-pixbuf") == 0) {
                        libdir = NULL;
                }
                else {
                        if (slash != NULL && g_ascii_strcasecmp (slash + 1, "bin") == 0) {
                                *slash = '\0';
                        }

                        libdir = g_build_filename (runtime_prefix,
                                                   pixbuf_libdir + strlen (pixbuf_prefix) + 1,
                                                   NULL);
                }
        }
        else {
                libdir = NULL;
        }

        g_free (pixbuf_prefix);

        if (libdir != NULL) {
                g_free (pixbuf_libdir);
                pixbuf_libdir = libdir;
        }
#endif

	/* This call is necessary to ensure we actually link against libgobject;
	 * otherwise it may be stripped if -Wl,--as-needed is in use.
	 * 
	 * The reason we need to link against libgobject is because it now has
	 * a global constructor.  If the dynamically loaded modules happen
	 * to dlclose() libgobject, then reopen it again, we're in for trouble.
	 *
	 * See: https://bugzilla.gnome.org/show_bug.cgi?id=686822
	 */
	g_type_ensure (G_TYPE_OBJECT);

        if (argc > 1 && strcmp (argv[1], "--update-cache") == 0) {
                cache_file = gdk_pixbuf_get_module_file ();
                first_file = 2;
        }

        contents = g_string_new ("");

        prgname = g_get_prgname ();
        g_string_append_printf (contents,
                                "# GdkPixbuf Image Loader Modules file\n"
                                "# Automatically generated file, do not edit\n"
                                "# Created by %s from gdk-pixbuf-%s\n"
                                "#\n",
                                (prgname ? prgname : "gdk-pixbuf-query-loaders"),
                                GDK_PIXBUF_VERSION);

        if (argc == first_file) {
#ifdef USE_GMODULE
                char *moduledir;
                GDir *dir;
                GList *l, *modules;

                moduledir = g_strdup (g_getenv ("GDK_PIXBUF_MODULEDIR"));
#ifdef G_OS_WIN32
                if (moduledir != NULL && *moduledir != '\0') {
                        gchar *path;

                        path = g_locale_to_utf8 (moduledir, -1, NULL, NULL, NULL);
                        g_free (moduledir);
                        moduledir = path;
                }
#endif
                if (moduledir == NULL || *moduledir == '\0') {
                        g_free (moduledir);
                        moduledir = g_strdup (pixbuf_libdir);
                }

                g_string_append_printf (contents, "# LoaderDir = %s\n#\n", moduledir);

                modules = NULL;
                dir = g_dir_open (moduledir, 0, NULL);
                if (dir) {
                        const char *dent;

                        while ((dent = g_dir_read_name (dir))) {
                                gint len = strlen (dent);
                                if (len > SOEXT_LEN &&
                                    strcmp (dent + len - SOEXT_LEN, SOEXT) == 0) {
                                        modules = g_list_prepend (modules,
                                                                  g_strdup (dent));
                                }
                        }
                        g_dir_close (dir);
                }
                modules = g_list_sort (modules, (GCompareFunc)strcmp);
                for (l = modules; l != NULL; l = l->next)
                        query_module (contents, moduledir, l->data);
                g_list_free_full (modules, g_free);
                g_free (moduledir);
#else
                g_string_append_printf (contents, "# dynamic loading of modules not supported\n");
#endif
        }
        else {
                char *cwd = g_get_current_dir ();

                for (i = first_file; i < argc; i++) {
                        char *infilename = argv[i];
#ifdef G_OS_WIN32
                        infilename = g_locale_to_utf8 (infilename,
                                                       -1, NULL, NULL, NULL);
#endif
                        query_module (contents, cwd, infilename);
                }
                g_free (cwd);
        }

        if (cache_file) {
                GError *err;

                err = NULL;
                if (!g_file_set_contents (cache_file, contents->str, -1, &err)) {
                        g_fprintf (stderr, "%s\n", err->message);
                }
        }
        else
                g_print ("%s\n", contents->str);

        g_string_free (contents, TRUE);

        g_free (pixbuf_libdir);

        return 0;
}
