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

#include <stdlib.h>

#include <adwaita.h>
#include <gio/gunixoutputstream.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <act/act.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "cc-avatar-chooser.h"
#include "cc-crop-area.h"
#include "user-utils.h"

#define ROW_SPAN 5
#define AVATAR_CHOOSER_PIXEL_SIZE 80

struct _CcAvatarChooser {
        GtkPopover parent;

        GtkWidget *crop_area;
        GtkWidget *flowbox;

        GnomeDesktopThumbnailFactory *thumb_factory;
        GListStore *faces;

        ActUser *user;
};

G_DEFINE_TYPE (CcAvatarChooser, cc_avatar_chooser, GTK_TYPE_POPOVER)

static void
crop_dialog_response (CcAvatarChooser *self,
                      gint             response_id,
                      GtkWidget       *dialog)
{
        g_autoptr(GdkPixbuf) pb = NULL;
        g_autoptr(GdkPixbuf) pb2 = NULL;
        g_autoptr(GdkTexture) texture = NULL;

        if (response_id != GTK_RESPONSE_ACCEPT) {
                self->crop_area = NULL;
                gtk_window_destroy (GTK_WINDOW (dialog));
                return;
        }

        pb = cc_crop_area_create_pixbuf (CC_CROP_AREA (self->crop_area));
        pb2 = gdk_pixbuf_scale_simple (pb, AVATAR_PIXEL_SIZE, AVATAR_PIXEL_SIZE, GDK_INTERP_BILINEAR);
        texture = gdk_texture_new_for_pixbuf (pb2);

        set_user_icon_data (self->user, texture, IMAGE_SOURCE_VALUE_CUSTOM);

        self->crop_area = NULL;
        gtk_window_destroy (GTK_WINDOW (dialog));

        gtk_popover_popdown (GTK_POPOVER (self));
}

static void
cc_avatar_chooser_crop (CcAvatarChooser *self,
                        GdkPixbuf       *pixbuf)
{
        GtkWidget *dialog;

        dialog = gtk_dialog_new_with_buttons ("",
                                              GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                                              GTK_DIALOG_USE_HEADER_BAR,
                                              _("_Cancel"),
                                              GTK_RESPONSE_CANCEL,
                                              _("Select"),
                                              GTK_RESPONSE_ACCEPT,
                                              NULL);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-users");

        g_signal_connect_object (G_OBJECT (dialog), "response",
                                 G_CALLBACK (crop_dialog_response), self, G_CONNECT_SWAPPED);

        /* Content */
        self->crop_area = cc_crop_area_new ();
        cc_crop_area_set_min_size (CC_CROP_AREA (self->crop_area), 48, 48);
        cc_crop_area_set_paintable (CC_CROP_AREA (self->crop_area),
                                    GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf)));
        gtk_box_prepend (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                         self->crop_area);
        gtk_widget_set_hexpand (self->crop_area, TRUE);
        gtk_widget_set_vexpand (self->crop_area, TRUE);

        gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);

        gtk_window_present (GTK_WINDOW (dialog));
}

static void
file_dialog_open_cb (GObject            *source_object,
                     GAsyncResult       *res,
                     gpointer            user_data)
{
        CcAvatarChooser *self = CC_AVATAR_CHOOSER (user_data);
        GtkFileDialog *file_dialog = GTK_FILE_DIALOG (source_object);
        g_autoptr(GError) error = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        g_autoptr(GdkPixbuf) pixbuf2 = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileInputStream) stream = NULL;

        file = gtk_file_dialog_open_finish (file_dialog, res, &error);

        if (error != NULL) {
                g_warning ("Failed to pick avatar image: %s", error->message);
                return;
        }

        stream = g_file_read (file, NULL, &error);
        pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream),
                                             NULL, &error);
        if (pixbuf == NULL) {
                g_warning ("Failed to load %s: %s", g_file_get_uri (file), error->message);
        }

        pixbuf2 = gdk_pixbuf_apply_embedded_orientation (pixbuf);

        cc_avatar_chooser_crop (self, pixbuf2);
}

static void
cc_avatar_chooser_select_file (CcAvatarChooser *self)
{
        g_autoptr(GFile) pictures_folder = NULL;
        GtkFileDialog *file_dialog;
        GtkFileFilter *filter;
        GListStore *filters;

        g_return_if_fail (CC_IS_AVATAR_CHOOSER (self));

        file_dialog = gtk_file_dialog_new ();
        gtk_file_dialog_set_title (file_dialog, _("Browse for more pictures"));
        gtk_file_dialog_set_modal (file_dialog, TRUE);

        filter = gtk_file_filter_new ();
        gtk_file_filter_add_pixbuf_formats (filter);

        filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
        g_list_store_append (filters, filter);
        gtk_file_dialog_set_filters (file_dialog, G_LIST_MODEL (filters));

        pictures_folder = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
        gtk_file_dialog_set_initial_folder (file_dialog, pictures_folder);

        gtk_popover_popdown (GTK_POPOVER (self));
        gtk_file_dialog_open (file_dialog,
                              GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                              NULL,
                              file_dialog_open_cb,
                              self);
}

static void
face_widget_activated (CcAvatarChooser *self,
                       GtkFlowBoxChild *child)
{
        const gchar *filename;
        GtkWidget   *image;
        g_autoptr(GdkTexture) texture = NULL;
        g_autoptr(GError) error = NULL;

        image = gtk_flow_box_child_get_child (child);
        filename = g_object_get_data (G_OBJECT (image), "filename");

        if (filename != NULL) {
                texture = gdk_texture_new_from_filename (filename, &error);
        }

        if (error != NULL) {
                g_warning ("Failed to load selected avatar image: %s", error->message);
        } else {
                set_user_icon_data (self->user, texture, IMAGE_SOURCE_VALUE_FACE);
        }

        gtk_popover_popdown (GTK_POPOVER (self));
}

static GtkWidget *
create_face_widget (gpointer item,
                    gpointer user_data)
{
        g_autofree gchar *image_path = NULL;
        g_autoptr(GdkTexture) source_image = NULL;
        GtkWidget *child = NULL;
        GtkWidget *avatar = NULL;

        image_path = g_file_get_path (G_FILE (item));

        avatar = adw_avatar_new (AVATAR_CHOOSER_PIXEL_SIZE, NULL, false);
        child = gtk_flow_box_child_new ();
        gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (child), avatar);

        if (image_path) {
                source_image = gdk_texture_new_from_filename (image_path, NULL);
                g_object_set_data_full (G_OBJECT (avatar),
                                        "filename", g_steal_pointer (&image_path), g_free);
        }
        if (source_image == NULL) {
                adw_avatar_set_icon_name (ADW_AVATAR (avatar), "image-missing");
        } else {
                adw_avatar_set_custom_image (ADW_AVATAR (avatar), GDK_PAINTABLE (source_image));
        }

        gtk_accessible_update_property (GTK_ACCESSIBLE (child),
                                        GTK_ACCESSIBLE_PROPERTY_LABEL, g_object_get_data (item, "a11y_label"),
                                        -1);

        return child;
}

static GStrv
get_settings_facesdirs (void)
{
        g_autoptr(GSettings) settings = g_settings_new ("org.gnome.desktop.interface");
        g_auto(GStrv) settings_dirs = g_settings_get_strv (settings, "avatar-directories");
        g_autoptr(GPtrArray) facesdirs = g_ptr_array_new ();

        if (settings_dirs != NULL) {
                int i;
                for (i = 0; settings_dirs[i] != NULL; i++) {
                        char *path = settings_dirs[i];
                        if (g_strcmp0 (path, "") != 0)
                                g_ptr_array_add (facesdirs, g_strdup (path));
                }
        }
        g_ptr_array_add (facesdirs, NULL);

        return (GStrv) g_ptr_array_steal (facesdirs, NULL);
}

static GStrv
get_system_facesdirs (void)
{
        const char * const * data_dirs;
        g_autoptr(GPtrArray) facesdirs = NULL;
        int i;

        facesdirs = g_ptr_array_new ();

        data_dirs = g_get_system_data_dirs ();
        for (i = 0; data_dirs[i] != NULL; i++) {
                char *path = g_build_filename (data_dirs[i], "pixmaps", "faces", NULL);
                g_ptr_array_add (facesdirs, path);
        }
        g_ptr_array_add (facesdirs, NULL);
        return (GStrv) g_ptr_array_steal (facesdirs, NULL);
}

static gboolean
add_faces_from_dirs (GListStore *faces, GStrv facesdirs, gboolean add_all)
{
        gboolean added_faces = FALSE;

        for (guint i = 0; facesdirs[i] != NULL; i++) {
                g_autoptr(GFile) dir = NULL;
                g_autoptr(GFileEnumerator) enumerator = NULL;

                dir = g_file_new_for_path (facesdirs[i]);

                enumerator = g_file_enumerate_children (dir,
                                                        G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                                        G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                                                        G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET ","
                                                        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                                        G_FILE_QUERY_INFO_NONE,
                                                        NULL, NULL);
                if (enumerator == NULL) {
                        continue;
                }

                while (TRUE) {
                        GFile *file;
                        GFileType type;
                        const gchar *target;
                        const gchar *display_name;
                        const gchar *last_dot;
                        g_autoptr(GFileInfo) info = g_file_enumerator_next_file (enumerator, NULL, NULL);
                        if (info == NULL) {
                                break;
                        }

                        type = g_file_info_get_file_type (info);
                        if (type != G_FILE_TYPE_REGULAR &&
                            type != G_FILE_TYPE_SYMBOLIC_LINK) {
                                continue;
                        }

                        target = g_file_info_get_attribute_byte_string (info,
                                                                        G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);
                        if (target != NULL && g_str_has_prefix (target , "legacy/")) {
                                continue;
                        }

                        file = g_file_get_child (dir, g_file_info_get_name (info));

                        display_name = g_file_info_get_display_name (info);
                        last_dot = g_strrstr (display_name, ".");
                        if (last_dot) {
                                g_object_set_data_full (G_OBJECT (file), "a11y_label",
                                                        g_strndup (display_name, last_dot - display_name), g_free);
                        } else {
                                g_object_set_data_full (G_OBJECT (file), "a11y_label",
                                                        g_strdup (display_name), g_free);
                        }

                        g_list_store_append (faces, file);

                        added_faces = TRUE;
                }

                g_file_enumerator_close (enumerator, NULL, NULL);

                if (added_faces && !add_all)
                        break;
        }
        return added_faces;
}


static void
setup_photo_popup (CcAvatarChooser *self)
{
        g_auto(GStrv) settings_facesdirs = NULL;

        self->faces = g_list_store_new (G_TYPE_FILE);
        gtk_flow_box_bind_model (GTK_FLOW_BOX (self->flowbox),
                                 G_LIST_MODEL (self->faces),
                                 create_face_widget,
                                 self,
                                 NULL);

        g_signal_connect_object (self->flowbox, "child-activated",
                                 G_CALLBACK (face_widget_activated), self, G_CONNECT_SWAPPED);

        settings_facesdirs = get_settings_facesdirs ();

        if (!add_faces_from_dirs (self->faces, settings_facesdirs, TRUE)) {
                g_auto(GStrv) system_facesdirs = get_system_facesdirs ();
                add_faces_from_dirs (self->faces, system_facesdirs, FALSE);
        }
}

CcAvatarChooser *
cc_avatar_chooser_new (void)
{
        return g_object_new (CC_TYPE_AVATAR_CHOOSER,
                             NULL);
}

static void
cc_avatar_chooser_dispose (GObject *object)
{
        CcAvatarChooser *self = CC_AVATAR_CHOOSER (object);

        g_clear_object (&self->thumb_factory);
        g_clear_object (&self->user);

        G_OBJECT_CLASS (cc_avatar_chooser_parent_class)->dispose (object);
}

static void
cc_avatar_chooser_init (CcAvatarChooser *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

        setup_photo_popup (self);
}

static void
cc_avatar_chooser_class_init (CcAvatarChooserClass *klass)
{
        GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
        GObjectClass *oclass = G_OBJECT_CLASS (klass);

        gtk_widget_class_set_template_from_resource (wclass, "/org/gnome/control-center/system/users/cc-avatar-chooser.ui");

        gtk_widget_class_bind_template_child (wclass, CcAvatarChooser, flowbox);

        gtk_widget_class_bind_template_callback (wclass, cc_avatar_chooser_select_file);

        oclass->dispose = cc_avatar_chooser_dispose;
}

void
cc_avatar_chooser_set_user (CcAvatarChooser *self,
                            ActUser         *user)
{
        g_return_if_fail (self != NULL);

        g_clear_object (&self->user);
        self->user = g_object_ref (user);
}
