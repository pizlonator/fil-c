/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
#include <utmpx.h>
#include <pwd.h>

#ifdef __FreeBSD__
#include <sysexits.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "user-utils.h"

/* Taken from defines.h in shadow-utils. On Linux, this value is much smaller
 * than the sysconf limit LOGIN_NAME_MAX, and values larger than this will
 * result in failure when running useradd. We could check UT_NAMESIZE instead,
 * but that is nonstandard. Better to use POSIX utmpx.
 */
gsize
get_username_max_length (void)
{
        return sizeof (((struct utmpx *)NULL)->ut_user);
}

gboolean
is_username_used (const gchar *username)
{
        struct passwd *pwent;

        if (username == NULL || username[0] == '\0') {
                return FALSE;
        }

        pwent = getpwnam (username);

        return pwent != NULL;
}

gboolean
is_valid_name (const gchar *name)
{
        gboolean is_empty = TRUE;
        gboolean found_comma = FALSE;
        const gchar *c;

        if (name == NULL)
                return is_empty;

        /* Valid names must contain:
         *   1) at least one character.
         *   2) at least one non-"space" character.
         *   3) comma character not allowed. Issue #888
         */
        for (c = name; *c; c++) {
                gunichar unichar;

                unichar = g_utf8_get_char_validated (c, -1);

                /* Partial UTF-8 sequence or end of string */
                if (unichar == (gunichar) -1 || unichar == (gunichar) -2)
                        break;

                /* Check for non-space character */
                if (is_empty && !g_unichar_isspace (unichar)) {
                        is_empty = FALSE;
                }

                if (unichar == ',') {
                        found_comma = TRUE;
                        break;
                }
        }

        return !is_empty && !found_comma;
}

typedef struct {
        gchar *username;
        gchar *tip;
} isValidUsernameData;

static void
is_valid_username_data_free (isValidUsernameData *data)
{
        g_clear_pointer (&data->username, g_free);
        g_clear_pointer (&data->tip, g_free);
        g_free (data);
}

#ifdef __FreeBSD__
/* Taken from pw(8) man page. */
#define E_SUCCESS EX_OK
#define E_BAD_ARG EX_DATAERR
#define E_NOTFOUND EX_NOUSER
#else
/* Taken from usermod.c in shadow-utils. */
#define E_SUCCESS 0
#define E_BAD_ARG 3
#define E_NOTFOUND 6
#endif

static void
is_valid_username_child_watch_cb (GPid pid,
                                  gint status,
                                  gpointer user_data)
{
        g_autoptr(GTask) task = G_TASK (user_data);
        isValidUsernameData *data = g_task_get_task_data (task);
        GError *error = NULL;
        gboolean valid = FALSE;
        const gchar *tip = NULL;

        if (WIFEXITED (status)) {
                switch (WEXITSTATUS (status)) {
                        case E_NOTFOUND:
                                valid = TRUE;
                                break;
                        case E_BAD_ARG:
                                tip = _("The username should usually only consist of lower case letters from a-z, digits and the following characters: - _");
                                valid = FALSE;
                                break;
                        case E_SUCCESS:
                                tip = _("Sorry, that user name isn’t available. Please try another.");
                                valid = FALSE;
                                break;
                }
        }

        if (valid || tip != NULL) {
                data->tip = g_strdup (tip);
                g_task_return_boolean (task, valid);
        }
        else {
                g_spawn_check_wait_status (status, &error);
                g_task_return_error (task, error);
        }

        g_spawn_close_pid (pid);
}

void
is_valid_username_async (const gchar *username,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer callback_data)
{
        g_autoptr(GTask) task = NULL;
        isValidUsernameData *data;
        gchar *argv[6];
        GPid pid;
        GError *error = NULL;

        task = g_task_new (NULL, cancellable, callback, callback_data);
        g_task_set_source_tag (task, is_valid_username_async);

        data = g_new0 (isValidUsernameData, 1);
        data->username = g_strdup (username);
        g_task_set_task_data (task, data, (GDestroyNotify) is_valid_username_data_free);

        if (username == NULL || username[0] == '\0') {
                g_task_return_boolean (task, FALSE);
                return;
        }
        else if (strlen (username) > get_username_max_length ()) {
                data->tip = g_strdup (_("The username is too long."));
                g_task_return_boolean (task, FALSE);
                return;
        }

#ifdef __FreeBSD__
        /* Abuse "pw usershow -n <name>" in the same way as the code below. We
         * don't use "pw usermod -n <name> -N -l <newname>" here because it has
         * a special case for "root" to reject changes to the root user.
         */
        argv[0] = "pw";
        argv[1] = "usershow";
        argv[2] = "-n";
        argv[3] = data->username;
        argv[4] = NULL;
#else
        /* "usermod --login" is meant to be used to change a username, but the
         * exit codes can be safely abused to check the validity of username.
         * However, the current "usermod" implementation may change in the
         * future, so it would be nice to have some official way for this
         * instead of relying on the current "--login" implementation.
         */
        argv[0] = "/usr/sbin/usermod";
        argv[1] = "--login";
        argv[2] = data->username;
        argv[3] = "--";
        argv[4] = data->username;
        argv[5] = NULL;
#endif

        if (!g_spawn_async (NULL, argv, NULL,
                            G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
                            G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                            NULL, NULL, &pid, &error)) {
                g_task_return_error (task, error);
                return;
        }

        g_child_watch_add (pid, (GChildWatchFunc) is_valid_username_child_watch_cb, task);
        g_steal_pointer (&task);
}

gboolean
is_valid_username_finish (GAsyncResult *result,
                          gchar **tip,
                          gchar **username,
                          GError **error)
{
        GTask *task;
        isValidUsernameData *data;

        g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

        task = G_TASK (result);
        data = g_task_get_task_data (task);

        if (tip != NULL) {
                *tip = g_steal_pointer (&data->tip);
        }

        if (username != NULL)
                *username = g_steal_pointer (&data->username);

        return g_task_propagate_boolean (task, error);
}

/* This function was taken from AdwAvatar and modified so that it's possible to
 * export a GdkTexture at a different size than the AdwAvatar is rendered
 * See: https://gitlab.gnome.org/GNOME/libadwaita/-/blob/afd0fab86ff9b4332d165b985a435ea6f822d41b/src/adw-avatar.c#L751
 * License: LGPL-2.1-or-later */
GdkTexture *
draw_avatar_to_texture (AdwAvatar *avatar, int size)
{
        GdkTexture *result;
        GskRenderNode *node;
        GtkSnapshot *snapshot;
        GdkPaintable *paintable;
        GtkNative *native;
        GskRenderer *renderer;
        int real_size;
        graphene_matrix_t transform;
        gboolean transform_ok;

        real_size = adw_avatar_get_size (avatar);

        /* This works around the issue that when the custom-image or text of the AdwAvatar changes the
         * allocation gets invalidated and therefore we can't snapshot the widget till the allocation
         * is recalculated */
        gtk_widget_measure (GTK_WIDGET (avatar), GTK_ORIENTATION_HORIZONTAL, real_size, NULL, NULL, NULL, NULL);
        gtk_widget_allocate (GTK_WIDGET (avatar), real_size, real_size, -1, NULL);

        transform_ok = gtk_widget_compute_transform (GTK_WIDGET (avatar),
                                                     gtk_widget_get_first_child (GTK_WIDGET (avatar)),
                                                     &transform);

        g_assert (transform_ok);

        snapshot = gtk_snapshot_new ();
        gtk_snapshot_transform_matrix (snapshot, &transform);
        GTK_WIDGET_GET_CLASS (avatar)->snapshot (GTK_WIDGET (avatar), snapshot);

        /* Create first a GdkPaintable at the size the avatar was drawn
         * then create a GdkSnapshot of it at the size requested */
        paintable = gtk_snapshot_free_to_paintable (snapshot, &GRAPHENE_SIZE_INIT (real_size, real_size));
        snapshot = gtk_snapshot_new ();
        gdk_paintable_snapshot (paintable, snapshot, size, size);
        g_object_unref (paintable);

        node = gtk_snapshot_free_to_node (snapshot);

        native = gtk_widget_get_native (GTK_WIDGET (avatar));
        renderer = gtk_native_get_renderer (native);

        result = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (-1, 0, size, size));

        gsk_render_node_unref (node);

        return result;
}

void
set_user_icon_data (ActUser     *user,
                    GdkTexture  *texture,
                    const gchar *image_source)
{
        g_autofree gchar *path = NULL;
        g_autoptr(GError) error = NULL;
        int fd;

        fd = g_file_open_tmp ("gnome-control-center-user-icon-XXXXXX", &path, &error);

        if (fd == -1) {
                g_warning ("Failed to create temporary user icon: %s", error->message);
                return;
        }

        g_autoptr(GdkPixbuf) pixbuf = gdk_pixbuf_get_from_texture (texture);
        gdk_pixbuf_save (pixbuf, path, "png", &error, IMAGE_SOURCE_KEY, image_source, NULL);

        if (error != NULL) {
            g_warning ("Failed to create temporary user icon: %s", error->message);
        }

        close (fd);

        act_user_set_icon_file (user, path);

        /* if we ever make the dbus call async, the g_remove call needs
         * to wait for its completion
         */
        g_remove (path);
}

const gchar *
get_real_or_user_name (ActUser *user)
{
  const gchar *name;

  name = act_user_get_real_name (user);
  if (name == NULL)
    name = act_user_get_user_name (user);

  return name;
}

void
setup_avatar_for_user (AdwAvatar *avatar, ActUser *user)
{
        const gchar *avatar_file;

        adw_avatar_set_custom_image (avatar, NULL);
        adw_avatar_set_text (avatar, get_real_or_user_name (user));

        avatar_file = act_user_get_icon_file (user);
        if (avatar_file) {
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                const gchar *image_source;
                gboolean is_generated = TRUE;

                pixbuf = gdk_pixbuf_new_from_file_at_size (avatar_file,
                                                           adw_avatar_get_size (avatar),
                                                           adw_avatar_get_size (avatar),
                                                           NULL);

                if (pixbuf) {
                        image_source = gdk_pixbuf_get_option (pixbuf, IMAGE_SOURCE_KEY);

                        if (image_source == NULL)
                                g_debug ("User avatar's source isn't defined");
                        else
                                g_debug ("User avatar's source is %s", image_source);

                        is_generated = g_strcmp0 (image_source, "gnome-generated") == 0;
                }

                if (!is_generated) {
                        g_autoptr(GdkTexture) texture = NULL;

                        texture = gdk_texture_new_for_pixbuf (pixbuf);
                        adw_avatar_set_custom_image (avatar, GDK_PAINTABLE (texture));
                }
        }
}

GSettings *
settings_or_null (const gchar *schema)
{
        GSettingsSchemaSource *source = NULL;
        g_auto(GStrv) non_relocatable = NULL;
        GSettings *settings = NULL;

        source = g_settings_schema_source_get_default ();
        if (!source)
                return NULL;

        g_settings_schema_source_list_schemas (source, TRUE, &non_relocatable, NULL);

        if (g_strv_contains ((const gchar * const *)non_relocatable, schema))
                settings = g_settings_new (schema);

        return settings;
}
