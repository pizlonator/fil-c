/* gtkaccessibleprivate.h: Accessible interface
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkaccessible.h"

G_BEGIN_DECLS

/* < private >
 * GtkAccessiblePlatformChange:
 * @GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSABLE: whether the accessible has changed
 *   its focusable state
 * @GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSED: whether the accessible has changed its
 *   focused state
 * @GTK_ACCESSIBLE_PLATFORM_CHANGE_ACTIVE: whether the accessible has changed its
 *   active state
 *
 * Represents the various platform changes which can occur and are communicated
 * using [method@Gtk.Accessible.platform_changed].
 */
typedef enum {
  GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSABLE = 1 << GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE,
  GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSED   = 1 << GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED,
  GTK_ACCESSIBLE_PLATFORM_CHANGE_ACTIVE    = 1 << GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE,
} GtkAccessiblePlatformChange;

typedef enum {
  GTK_ACCESSIBLE_CHILD_STATE_ADDED,
  GTK_ACCESSIBLE_CHILD_STATE_REMOVED
} GtkAccessibleChildState;

typedef enum {
  GTK_ACCESSIBLE_CHILD_CHANGE_ADDED   = 1 << GTK_ACCESSIBLE_CHILD_STATE_ADDED,
  GTK_ACCESSIBLE_CHILD_CHANGE_REMOVED = 1 << GTK_ACCESSIBLE_CHILD_STATE_REMOVED
} GtkAccessibleChildChange;

const char *    gtk_accessible_role_to_name     (GtkAccessibleRole  role,
                                                 const char        *domain);

gboolean        gtk_accessible_role_is_range_subclass (GtkAccessibleRole role);

gboolean        gtk_accessible_role_is_subclass       (GtkAccessibleRole role,
                                                       GtkAccessibleRole superclass);

gboolean        gtk_accessible_role_is_abstract       (GtkAccessibleRole role);

/* < private >
 * GtkAccessibleNaming:
 * @GTK_ACCESSIBLE_NAME_ALLOWED:
 *   The role allows an accessible name and description
 * @GTK_ACCESSIBLE_NAME_PROHIBITED:
 *   The role does not allow an accessible name and description
 * @GTK_ACCESSIBLE_NAME_REQUIRED:
 *   The role requires an accessible name and description
 * @GTK_ACCESSIBLE_NAME_RECOMMENDED:
 *   It is recommended to set the label property or labelled-by relation
 *   for this role
 * @GTK_ACCESSIBLE_NAME_NOT_RECOMMENDED:
 *   It is recommended not to set the label property or labelled-by relation
 *   for this role
 *
 * Information about naming requirements for accessible roles.
 */
typedef enum {
  GTK_ACCESSIBLE_NAME_ALLOWED,
  GTK_ACCESSIBLE_NAME_PROHIBITED,
  GTK_ACCESSIBLE_NAME_REQUIRED,
  GTK_ACCESSIBLE_NAME_RECOMMENDED,
  GTK_ACCESSIBLE_NAME_NOT_RECOMMENDED,
} GtkAccessibleNaming;

gboolean gtk_accessible_role_supports_name_from_author  (GtkAccessibleRole role) G_GNUC_CONST;
gboolean gtk_accessible_role_supports_name_from_content (GtkAccessibleRole role) G_GNUC_CONST;
GtkAccessibleNaming gtk_accessible_role_get_naming      (GtkAccessibleRole role) G_GNUC_CONST;

gboolean        gtk_accessible_should_present   (GtkAccessible     *self);

void            gtk_accessible_update_children  (GtkAccessible           *self,
                                                 GtkAccessible           *child,
                                                 GtkAccessibleChildState  state);

void            gtk_accessible_bounds_changed   (GtkAccessible *self);

void            gtk_accessible_platform_changed (GtkAccessible                *self,
                                                 GtkAccessiblePlatformChange   change);

G_END_DECLS
