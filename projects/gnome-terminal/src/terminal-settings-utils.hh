/*
 * Copyright © 2008, 2010, 2022 Christian Persch
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

#pragma once

#include <gio/gio.h>

GSettings* terminal_g_settings_new (GSettingsBackend* backend,
                                    GSettingsSchemaSource* source,
                                    char const* schema_id);

GSettings* terminal_g_settings_new_with_path (GSettingsBackend* backend,
                                              GSettingsSchemaSource* source,
                                              char const* schema_id,
                                              char const* path);

void terminal_g_settings_backend_clone_schema(GSettingsBackend* backend,
                                              GSettingsSchemaSource*schema_source,
                                              char const* schema_id,
                                              char const* path,
                                              char const* new_path,
                                              GTree* tree);

gboolean terminal_g_settings_backend_erase_path(GSettingsBackend* backend,
                                                GSettingsSchemaSource* schema_source,
                                                char const* schema_id,
                                                char const* path);

GTree* terminal_g_settings_backend_create_tree(void);

void terminal_g_settings_backend_print_tree(GTree* tree);

GSettingsSchemaSource* terminal_g_settings_schema_source_get_default(void);

GTree* terminal_g_settings_backend_create_tree(void);

// BEGIN copied from glib/gio/gsettingsbackendinternal.h

/*
 * Copyright © 2009, 2010 Codethink Limited
 * Copyright © 2010 Red Hat, Inc.
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
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Matthias Clasen <mclasen@redhat.com>
 */

GPermission* terminal_g_settings_backend_get_permission(GSettingsBackend* backend,
                                                        char const*path);

gboolean terminal_g_settings_backend_get_writable(GSettingsBackend* backend,
                                                  const char* key);

GVariant* terminal_g_settings_backend_read(GSettingsBackend* backend,
                                           char const* key,
                                           GVariantType const* expected_type,
                                           gboolean default_value);

GVariant* terminal_g_settings_backend_read_user_value(GSettingsBackend* backend,
                                                      char const* key,
                                                      GVariantType const* expected_type);

void terminal_g_settings_backend_reset(GSettingsBackend* backend,
                                       char const* key,
                                       void* origin_tag);

void terminal_g_settings_backend_subscribe(GSettingsBackend* backend,
                                           const char* name);

void terminal_g_settings_backend_sync(GSettingsBackend* backend);

void terminal_g_settings_backend_unsubscribe(GSettingsBackend* backend,
                                             const char* name);

gboolean terminal_g_settings_backend_write(GSettingsBackend* backend,
                                           char const* key,
                                           GVariant* value,
                                           void* origin_tag);

gboolean terminal_g_settings_backend_write_tree(GSettingsBackend* backend,
                                                GTree* tree,
                                                void* origin_tag);

// END copied from glib

GVariant* terminal_g_variant_wrap(GVariant* variant);

GVariant* terminal_g_variant_unwrap(GVariant* variant);
