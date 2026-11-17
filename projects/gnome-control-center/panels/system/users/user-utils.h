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

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <act/act.h>

G_BEGIN_DECLS

#define AVATAR_PIXEL_SIZE 512

/* Key and values that are written as metadata to the exported user avatar this
 * way it's possible to know how the image was initially created.
 * If set to generated we can regenerated the avatar when the style changes or
 * when the users full name changes. The other two values don't have a specific use yet */
#define IMAGE_SOURCE_KEY "tEXt::source"
#define IMAGE_SOURCE_VALUE_GENERATED "gnome-generated"
#define IMAGE_SOURCE_VALUE_FACE "gnome-face"
#define IMAGE_SOURCE_VALUE_CUSTOM "gnome-custom"

void     set_entry_generation_icon        (GtkEntry    *entry);
void     set_entry_validation_checkmark   (GtkEntry    *entry);
void     set_entry_validation_error       (GtkEntry    *entry,
                                           const gchar *text);
void     clear_entry_validation_error     (GtkEntry    *entry);

const gchar *get_real_or_user_name        (ActUser *user);
gsize    get_username_max_length          (void);
gboolean is_username_used                 (const gchar *username);
gboolean is_valid_name                    (const gchar *name);
void     is_valid_username_async          (const gchar *username,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer callback_data);
gboolean is_valid_username_finish         (GAsyncResult *result,
                                           gchar **tip,
                                           gchar **username,
                                           GError **error);
GdkTexture *draw_avatar_to_texture        (AdwAvatar *avatar,
                                           int        size);
void       set_user_icon_data             (ActUser     *user,
                                           GdkTexture  *texture,
                                           const gchar *image_source);
void       setup_avatar_for_user          (AdwAvatar *avatar,
                                           ActUser *user);
GSettings *settings_or_null               (const gchar *schema);

G_END_DECLS
